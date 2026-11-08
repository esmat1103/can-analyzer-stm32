#include "mainwindow.h"

#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QFrame>
#include <QCheckBox>
#include <QTimer>
#include <QSerialPortInfo>
#include <QRegularExpression>
#include <QFontDatabase>
#include <QTime>
#include <QPainter>
#include <QPainterPath>

static const int NB_MAX_LIGNES = 2000;   // taille max du tableau "flux"

// Couleurs (les memes que l'ecran de la F7)
static const char *COULEUR_TEXTE      = "#1E293B";
static const char *COULEUR_SECONDAIRE = "#64748B";
static const char *COULEUR_BORDURE    = "#D9DFE7";
static const char *COULEUR_ACCENT     = "#2563EB";
static const char *COULEUR_PHARES     = "#F59E0B";

// ===========================================================================
//  Courbe
// ===========================================================================

Courbe::Courbe(const QString &titre, const QString &unite, double maximum,
               const QColor &couleur, QWidget *parent)
    : QWidget(parent), m_titre(titre), m_unite(unite), m_max(maximum), m_couleur(couleur)
{
    setMinimumHeight(170);
}

void Courbe::ajouterPoint(double temps_s, double valeur)
{
    m_points.append(QPointF(temps_s, valeur));
    // On ne garde que les points des 30 dernieres secondes (+ une petite marge)
    while (!m_points.isEmpty() && m_points.first().x() < temps_s - FENETRE_S - 1.0)
        m_points.removeFirst();
    update();   // demande a Qt de redessiner le widget
}

void Courbe::effacer()
{
    m_points.clear();
    update();
}

void Courbe::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Cadre blanc arrondi
    p.setPen(QColor(COULEUR_BORDURE));
    p.setBrush(Qt::white);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);

    // Titre a gauche, derniere valeur a droite
    QFont police = font();
    police.setBold(true);
    p.setFont(police);
    p.setPen(QColor(COULEUR_TEXTE));
    p.drawText(QRectF(14, 6, width() - 28, 22), Qt::AlignLeft | Qt::AlignVCenter, m_titre);
    if (!m_points.isEmpty()) {
        p.setPen(m_couleur);
        p.drawText(QRectF(14, 6, width() - 28, 22), Qt::AlignRight | Qt::AlignVCenter,
                   QString("%1 %2").arg(qRound(m_points.last().y())).arg(m_unite));
    }

    // Zone du graphique
    const QRectF zone(52, 34, width() - 66, height() - 58);
    if (zone.width() < 20 || zone.height() < 20)
        return;

    police.setBold(false);
    if (police.pointSizeF() > 0)
        police.setPointSizeF(police.pointSizeF() * 0.85);
    p.setFont(police);

    // Grille horizontale et graduations de l'axe vertical
    for (int i = 0; i <= 4; ++i) {
        const double y = zone.bottom() - i * zone.height() / 4.0;
        p.setPen(QPen(QColor("#E2E8F0"), 1));
        p.drawLine(QPointF(zone.left(), y), QPointF(zone.right(), y));
        p.setPen(QColor(COULEUR_SECONDAIRE));
        p.drawText(QRectF(0, y - 8, zone.left() - 6, 16), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(qRound(m_max * i / 4.0)));
    }

    // Graduations de l'axe du temps : -30 s ... 0 s
    for (int s = 0; s <= 30; s += 10) {
        const double x = zone.right() - s / FENETRE_S * zone.width();
        p.drawText(QRectF(x - 22, zone.bottom() + 3, 44, 16), Qt::AlignCenter,
                   s == 0 ? QString("0 s") : QString("-%1 s").arg(s));
    }

    if (m_points.size() < 2)
        return;

    // Construction de la courbe : le point le plus recent est a droite
    const double tFin = m_points.last().x();
    QPainterPath chemin;
    for (int i = 0; i < m_points.size(); ++i) {
        const QPointF &pt = m_points.at(i);
        const double x = zone.right() - (tFin - pt.x()) / FENETRE_S * zone.width();
        const double v = qBound(0.0, pt.y(), m_max);
        const double y = zone.bottom() - v / m_max * zone.height();
        if (i == 0)
            chemin.moveTo(x, y);
        else
            chemin.lineTo(x, y);
    }

    p.setClipRect(zone);

    // Surface coloree sous la courbe
    QPainterPath surface = chemin;
    surface.lineTo(chemin.currentPosition().x(), zone.bottom());
    surface.lineTo(chemin.elementAt(0).x, zone.bottom());
    surface.closeSubpath();
    QColor remplissage = m_couleur;
    remplissage.setAlpha(45);
    p.setPen(Qt::NoPen);
    p.setBrush(remplissage);
    p.drawPath(surface);

    // Trait de la courbe
    p.setPen(QPen(m_couleur, 2.2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(chemin);
}

// ===========================================================================
//  Construction de la fenetre
// ===========================================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("CAN Analyzer - STM32");
    resize(1150, 800);

    // Feuille de style : theme clair, memes couleurs que l'ecran de la F7
    setStyleSheet(QString(R"(
        QMainWindow, QWidget#central { background: #EEF1F5; }
        QGroupBox { font-weight: 600; color: %1; background: #FFFFFF;
                    border: 1px solid %2; border-radius: 8px; margin-top: 12px; padding: 8px; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
        QFrame#carte { background: #FFFFFF; border: 1px solid %2; border-radius: 10px; }
        QLabel#titreCarte { color: %3; font-size: 13px; }
        QLabel#valeurCarte { color: %1; font-size: 32px; font-weight: 700; }
        QLabel#uniteCarte { color: %3; font-size: 13px; }
        QPushButton { background: %4; color: white; border: none; border-radius: 6px;
                      padding: 7px 16px; font-weight: 600; }
        QPushButton:hover { background: #1D4ED8; }
        QPushButton:disabled { background: #CBD5E1; color: #F8FAFC; }
        QPushButton#boutonPhares { background: %5; }
        QPushButton#boutonPhares:hover { background: #D97706; }
        QPushButton#boutonSecondaire { background: #64748B; }
        QPushButton#boutonSecondaire:hover { background: #475569; }
        QPushButton#boutonPhares:disabled, QPushButton#boutonSecondaire:disabled
                     { background: #CBD5E1; color: #F8FAFC; }
        QTabWidget::pane { border: 1px solid %2; border-radius: 8px; background: #F8FAFC; top: -1px; }
        QTabBar::tab { background: #E2E8F0; color: #334155; padding: 8px 18px; margin-right: 2px;
                       min-width: 110px; border-top-left-radius: 6px; border-top-right-radius: 6px; }
        QTabBar::tab:selected { background: #FFFFFF; color: %4; }
        QTableWidget { background: #FFFFFF; gridline-color: #E2E8F0; border: 1px solid %2;
                       selection-background-color: #DBEAFE; selection-color: %1; }
        QHeaderView::section { background: #F1F5F9; color: #475569; border: none;
                               border-bottom: 1px solid %2; padding: 5px; font-weight: 600; }
        QLineEdit, QComboBox { background: #FFFFFF; border: 1px solid #CBD5E1; border-radius: 6px;
                               padding: 5px 7px; color: %1; }
        QPlainTextEdit { background: #0F172A; color: #E2E8F0; border-radius: 6px; padding: 4px; }
    )").arg(COULEUR_TEXTE, COULEUR_BORDURE, COULEUR_SECONDAIRE, COULEUR_ACCENT, COULEUR_PHARES));

    const QFont policeFixe = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    auto *central = new QWidget(this);
    central->setObjectName("central");
    auto *layoutPrincipal = new QVBoxLayout(central);
    layoutPrincipal->setSpacing(10);

    // ---------- Connexion ----------
    auto *groupeConnexion = new QGroupBox("Connexion", central);
    auto *layoutConnexion = new QHBoxLayout(groupeConnexion);
    m_choixPort = new QComboBox;
    m_choixPort->setMinimumWidth(340);
    m_boutonRafraichir = new QPushButton("Actualiser");
    m_boutonRafraichir->setObjectName("boutonSecondaire");
    m_boutonConnexion = new QPushButton("Connecter");
    m_etat = new QLabel;
    layoutConnexion->addWidget(new QLabel("Port :"));
    layoutConnexion->addWidget(m_choixPort);
    layoutConnexion->addWidget(m_boutonRafraichir);
    layoutConnexion->addWidget(m_boutonConnexion);
    layoutConnexion->addStretch();
    layoutConnexion->addWidget(m_etat);
    layoutPrincipal->addWidget(groupeConnexion);

    // ---------- Onglets ----------
    auto *onglets = new QTabWidget(central);
    onglets->setUsesScrollButtons(false);
    onglets->addTab(creerTableauDeBord(), "Tableau de bord");
    onglets->addTab(creerOngletFlux(), "Trames (flux)");
    onglets->addTab(creerOngletParId(), "Vue par ID");
    layoutPrincipal->addWidget(onglets, 1);

    // ---------- Bas : envoi manuel + journal ----------
    auto *layoutBas = new QHBoxLayout;

    auto *groupeEnvoi = new QGroupBox("Envoyer une trame", central);
    auto *layoutEnvoi = new QGridLayout(groupeEnvoi);
    m_saisieId = new QLineEdit;
    m_saisieId->setPlaceholderText("ex. 500");
    m_saisieId->setFont(policeFixe);
    m_saisieDonnees = new QLineEdit;
    m_saisieDonnees->setPlaceholderText("ex. 01   (0 à 8 octets hexa, séparés par des espaces)");
    m_saisieDonnees->setFont(policeFixe);
    m_boutonEnvoyer = new QPushButton("Envoyer");
    layoutEnvoi->addWidget(new QLabel("ID (hex) :"), 0, 0);
    layoutEnvoi->addWidget(m_saisieId, 0, 1);
    layoutEnvoi->addWidget(new QLabel("Données :"), 1, 0);
    layoutEnvoi->addWidget(m_saisieDonnees, 1, 1);
    layoutEnvoi->addWidget(m_boutonEnvoyer, 2, 1, Qt::AlignRight);
    layoutBas->addWidget(groupeEnvoi, 2);

    auto *groupeJournal = new QGroupBox("Journal", central);
    auto *layoutJournal = new QVBoxLayout(groupeJournal);
    m_journal = new QPlainTextEdit;
    m_journal->setReadOnly(true);
    m_journal->setMaximumBlockCount(500);
    m_journal->setFixedHeight(105);
    m_journal->setFont(policeFixe);
    layoutJournal->addWidget(m_journal);
    layoutBas->addWidget(groupeJournal, 3);

    layoutPrincipal->addLayout(layoutBas);
    setCentralWidget(central);

    // ---------- Liaisons signaux -> fonctions ----------
    connect(m_boutonRafraichir, &QPushButton::clicked, this, &MainWindow::rafraichirPorts);
    connect(m_boutonConnexion, &QPushButton::clicked, this, &MainWindow::basculerConnexion);
    connect(m_boutonEnvoyer, &QPushButton::clicked, this, &MainWindow::envoyerTrame);
    connect(m_saisieDonnees, &QLineEdit::returnPressed, this, &MainWindow::envoyerTrame);
    connect(m_boutonPharesOn, &QPushButton::clicked, this, [this]() { envoyerCommande(0x500, {0x01}); });
    connect(m_boutonPharesOff, &QPushButton::clicked, this, [this]() { envoyerCommande(0x500, {0x00}); });
    connect(m_saisieFiltre, &QLineEdit::textChanged, this, &MainWindow::lireFiltre);
    connect(&m_port, &QSerialPort::readyRead, this, &MainWindow::lireDonnees);
    connect(&m_port, &QSerialPort::errorOccurred, this, &MainWindow::erreurPort);

    // Toutes les 500 ms : etat du bus et fin du surlignage des donnees modifiees
    m_minuteur = new QTimer(this);
    connect(m_minuteur, &QTimer::timeout, this, &MainWindow::verifierBus);
    m_minuteur->start(500);

    mettreAJourEtat(false);
    afficherPhares(-1);
    rafraichirPorts();
}

MainWindow::~MainWindow()
{
    if (m_port.isOpen())
        m_port.close();
}

// Une "carte" : titre, grande valeur, unite
QLabel *MainWindow::creerCarte(QWidget *parent, QHBoxLayout *layout,
                               const QString &titre, const QString &unite)
{
    auto *carte = new QFrame(parent);
    carte->setObjectName("carte");
    auto *l = new QVBoxLayout(carte);
    l->setContentsMargins(16, 10, 16, 10);
    auto *labelTitre = new QLabel(titre);
    labelTitre->setObjectName("titreCarte");
    auto *labelValeur = new QLabel("--");
    labelValeur->setObjectName("valeurCarte");
    auto *labelUnite = new QLabel(unite);
    labelUnite->setObjectName("uniteCarte");
    l->addWidget(labelTitre);
    l->addWidget(labelValeur);
    l->addWidget(labelUnite);
    layout->addWidget(carte, 1);
    return labelValeur;
}

QWidget *MainWindow::creerTableauDeBord()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(12);

    // Ligne 1 : les valeurs decodees
    auto *ligneCartes = new QHBoxLayout;
    m_valVitesse   = creerCarte(page, ligneCartes, "Vitesse", "km/h");
    m_valRegime    = creerCarte(page, ligneCartes, "Régime moteur", "tr/min");
    m_valTemp      = creerCarte(page, ligneCartes, "Température moteur", "°C");
    m_valBatterie  = creerCarte(page, ligneCartes, "Batterie", "V");
    m_valCarburant = creerCarte(page, ligneCartes, "Carburant", "%");
    layout->addLayout(ligneCartes);

    // Ligne 2 : les courbes
    auto *ligneCourbes = new QHBoxLayout;
    m_courbeVitesse = new Courbe("Vitesse (trame 0x200)", "km/h", 140, QColor(COULEUR_ACCENT), page);
    m_courbeRegime  = new Courbe("Régime (trame 0x100)", "tr/min", 4000, QColor("#0EA5E9"), page);
    ligneCourbes->addWidget(m_courbeVitesse, 1);
    ligneCourbes->addWidget(m_courbeRegime, 1);
    layout->addLayout(ligneCourbes, 1);

    // Ligne 3 : phares + etat du bus
    auto *cartePhares = new QFrame(page);
    cartePhares->setObjectName("carte");
    auto *lp = new QHBoxLayout(cartePhares);
    lp->setContentsMargins(16, 10, 16, 10);
    auto *titrePhares = new QLabel("Phares (trame 0x500)");
    titrePhares->setObjectName("titreCarte");
    m_voyantPhares = new QLabel;
    m_voyantPhares->setFixedSize(22, 22);
    m_textePhares = new QLabel;
    m_textePhares->setStyleSheet(QString("color: %1; font-size: 15px; font-weight: 600;").arg(COULEUR_TEXTE));
    m_boutonPharesOn = new QPushButton("Allumer");
    m_boutonPharesOn->setObjectName("boutonPhares");
    m_boutonPharesOff = new QPushButton("Éteindre");
    m_boutonPharesOff->setObjectName("boutonSecondaire");
    m_etatBus = new QLabel;
    lp->addWidget(titrePhares);
    lp->addSpacing(12);
    lp->addWidget(m_voyantPhares);
    lp->addWidget(m_textePhares);
    lp->addSpacing(12);
    lp->addWidget(m_boutonPharesOn);
    lp->addWidget(m_boutonPharesOff);
    lp->addStretch();
    lp->addWidget(m_etatBus);
    layout->addWidget(cartePhares);

    return page;
}

QWidget *MainWindow::creerOngletFlux()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    auto *barre = new QHBoxLayout;
    m_saisieFiltre = new QLineEdit;
    m_saisieFiltre->setPlaceholderText("ex. 100   ou   100, 200   (vide = tout afficher)");
    m_saisieFiltre->setMaximumWidth(340);
    m_pause = new QCheckBox("Pause");
    auto *boutonEffacer = new QPushButton("Effacer");
    boutonEffacer->setObjectName("boutonSecondaire");
    barre->addWidget(new QLabel("Filtre d'affichage (ID) :"));
    barre->addWidget(m_saisieFiltre);
    barre->addSpacing(16);
    barre->addWidget(m_pause);
    barre->addStretch();
    barre->addWidget(boutonEffacer);
    layout->addLayout(barre);

    m_table = new QTableWidget(0, 5);
    m_table->setHorizontalHeaderLabels({"Temps (s)", "ID", "DLC", "Données", "Signification"});
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setColumnWidth(0, 100);
    m_table->setColumnWidth(1, 80);
    m_table->setColumnWidth(2, 50);
    m_table->setColumnWidth(3, 220);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(m_table);

    connect(boutonEffacer, &QPushButton::clicked, this, [this]() { m_table->setRowCount(0); });
    return page;
}

QWidget *MainWindow::creerOngletParId()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    auto *aide = new QLabel("Une ligne par identifiant, mise à jour en direct. "
                            "Les données qui viennent de changer sont surlignées.");
    aide->setStyleSheet(QString("color: %1;").arg(COULEUR_SECONDAIRE));
    layout->addWidget(aide);

    m_tableId = new QTableWidget(0, 6);
    m_tableId->setHorizontalHeaderLabels({"ID", "DLC", "Données", "Signification", "Nombre", "Période"});
    m_tableId->verticalHeader()->setVisible(false);
    m_tableId->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_tableId->setColumnWidth(0, 80);
    m_tableId->setColumnWidth(1, 50);
    m_tableId->setColumnWidth(2, 220);
    m_tableId->setColumnWidth(4, 90);
    m_tableId->setColumnWidth(5, 90);
    m_tableId->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableId->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableId->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(m_tableId);

    return page;
}

// ===========================================================================
//  Port serie
// ===========================================================================

// Liste les ports serie et preselectionne celui de la carte s'il est reconnu.
void MainWindow::rafraichirPorts()
{
    m_choixPort->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        m_choixPort->addItem(info.portName() + " - " + info.description(), info.portName());
        const QString d = info.description() + " " + info.manufacturer();
        if (d.contains("STLink", Qt::CaseInsensitive) || d.contains("STMicro", Qt::CaseInsensitive)
            || d.contains("CP210"))
            m_choixPort->setCurrentIndex(m_choixPort->count() - 1);
    }
    if (ports.isEmpty())
        journal("Aucun port série trouvé. Branchez la carte F4 puis cliquez sur Actualiser.");
}

// Ouvre ou ferme le port serie.
void MainWindow::basculerConnexion()
{
    if (m_port.isOpen()) {
        m_port.close();
        mettreAJourEtat(false);
        journal("Déconnecté.");
        return;
    }

    const QString nomPort = m_choixPort->currentData().toString();
    if (nomPort.isEmpty()) {
        journal("Choisissez d'abord un port.");
        return;
    }

    m_port.setPortName(nomPort);
    m_port.setBaudRate(QSerialPort::Baud115200);
    m_port.setDataBits(QSerialPort::Data8);
    m_port.setParity(QSerialPort::NoParity);
    m_port.setStopBits(QSerialPort::OneStop);
    m_port.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port.open(QIODevice::ReadWrite)) {
        journal("Impossible d'ouvrir " + nomPort + " : " + m_port.errorString()
                + " (un autre logiciel utilise-t-il ce port ?)");
        return;
    }

    // Vide la ligne en attente dans la F4 : evite le "ERR commande inconnue"
    // provoque par des caracteres parasites recus a l'ouverture du port.
    m_port.write("\r\n");

    toutEffacer();
    m_tampon.clear();
    m_chrono.start();
    mettreAJourEtat(true);
    journal("Connecté à " + nomPort + " (115200 bauds).");
}

// Appelee chaque fois que des caracteres arrivent : on decoupe en lignes.
void MainWindow::lireDonnees()
{
    m_tampon += m_port.readAll();

    int fin;
    while ((fin = m_tampon.indexOf('\n')) >= 0) {
        const QByteArray ligne = m_tampon.left(fin);
        m_tampon.remove(0, fin + 1);
        traiterLigne(QString::fromLatin1(ligne).trimmed());
    }

    // Securite : si aucune fin de ligne n'arrive, on ne garde pas tout indefiniment
    if (m_tampon.size() > 1024)
        m_tampon.clear();
}

// Gere les erreurs du port, en particulier le debranchement de la carte.
void MainWindow::erreurPort(QSerialPort::SerialPortError erreur)
{
    if (erreur == QSerialPort::ResourceError) {
        journal("Liaison perdue : la carte F4 a-t-elle été débranchée ?");
        m_port.close();
        mettreAJourEtat(false);
    }
}

// ===========================================================================
//  Reception
// ===========================================================================

// Interprete une ligne envoyee par la F4.
void MainWindow::traiterLigne(const QString &ligne)
{
    if (ligne.isEmpty())
        return;

    if (ligne.startsWith("R ")) {
        // Format : R <id> <dlc> <octet1> ... <octetN>
        const QStringList morceaux = ligne.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        bool ok = false;
        const uint id = morceaux.size() >= 3 ? morceaux.at(1).toUInt(&ok, 16) : 0;
        const int dlc = ok ? morceaux.at(2).toInt(&ok) : -1;
        if (!ok || dlc < 0 || dlc > 8 || morceaux.size() != 3 + dlc) {
            journal("Ligne incorrecte ignorée : " + ligne);
            return;
        }
        QVector<quint8> data;
        for (int i = 0; i < dlc; ++i) {
            const uint v = morceaux.at(3 + i).toUInt(&ok, 16);
            if (!ok || v > 0xFF) {
                journal("Octet incorrect ignoré : " + ligne);
                return;
            }
            data.append(static_cast<quint8>(v));
        }
        traiterTrame(id, data);
    }
    else if (ligne.startsWith('#')) {
        const QString message = ligne.mid(1).trimmed();

        // Juste apres la connexion, le "\r\n" envoye vide la ligne parasite de la F4,
        // qui repond alors "ERR commande inconnue" : c'est attendu, on ne l'affiche pas.
        if (message.startsWith("ERR commande inconnue") && m_chrono.elapsed() < 500)
            return;

        journal("F4 : " + message);

        // La F4 confirme l'envoi de la commande des phares : on met a jour le voyant
        if (m_pharesEnAttente >= 0) {
            if (message.startsWith("OK T 500"))
                afficherPhares(m_pharesEnAttente);
            if (message.startsWith("OK") || message.startsWith("ERR"))
                m_pharesEnAttente = -1;
        }
    }
    else {
        journal("Ligne inconnue : " + ligne);
    }
}

// Une trame valide vient d'arriver : on met a jour les trois onglets.
void MainWindow::traiterTrame(uint id, const QVector<quint8> &data)
{
    m_nbTrames++;
    m_derniereTrame_ms = m_chrono.elapsed();

    mettreAJourTableauDeBord(id, data);
    mettreAJourVueParId(id, data);
    ajouterAuFlux(id, data);
}

// Traduit les octets d'une trame en valeurs physiques (notre "base de donnees" CAN).
QString MainWindow::decoder(uint id, const QVector<quint8> &d) const
{
    switch (id) {
    case 0x100:   // moteur : regime sur 2 octets (poids fort d'abord) + temperature + 40
        if (d.size() >= 3)
            return QString("Régime %1 tr/min · Moteur %2 °C")
                .arg((d[0] << 8) | d[1]).arg(int(d[2]) - 40);
        break;
    case 0x200:   // ABS : vitesse en km/h
        if (d.size() >= 1)
            return QString("Vitesse %1 km/h").arg(d[0]);
        break;
    case 0x300:   // batterie en centiemes de volt sur 2 octets + carburant en %
        if (d.size() >= 3)
            return QString("Batterie %1 V · Carburant %2 %")
                .arg(((d[0] << 8) | d[1]) / 100.0, 0, 'f', 2).arg(d[2]);
        break;
    case 0x500:   // commande des phares
        if (d.size() >= 1)
            return d[0] ? "Phares : allumer" : "Phares : éteindre";
        break;
    default:
        return "Trame inconnue";
    }
    return "Longueur inattendue";
}

void MainWindow::mettreAJourTableauDeBord(uint id, const QVector<quint8> &d)
{
    const double t = m_chrono.elapsed() / 1000.0;

    if (id == 0x100 && d.size() >= 3) {
        const int regime = (d[0] << 8) | d[1];
        m_valRegime->setText(QString::number(regime));
        m_valTemp->setText(QString::number(int(d[2]) - 40));
        m_courbeRegime->ajouterPoint(t, regime);
    }
    else if (id == 0x200 && d.size() >= 1) {
        m_valVitesse->setText(QString::number(d[0]));
        m_courbeVitesse->ajouterPoint(t, d[0]);
    }
    else if (id == 0x300 && d.size() >= 3) {
        m_valBatterie->setText(QString::number(((d[0] << 8) | d[1]) / 100.0, 'f', 2));
        m_valCarburant->setText(QString::number(d[2]));
    }
}

// Onglet "Vue par ID" : une ligne par identifiant, triee par ID croissant.
void MainWindow::mettreAJourVueParId(uint id, const QVector<quint8> &data)
{
    const qint64 maintenant = m_chrono.elapsed();

    QStringList octets;
    for (quint8 o : data)
        octets << QString("%1").arg(o, 2, 16, QChar('0')).toUpper();
    const QString texteDonnees = octets.join(' ');

    InfoId &info = m_infos[id];
    info.nombre++;
    if (info.dernier_ms >= 0) {
        const double ecart = double(maintenant - info.dernier_ms);
        // Moyenne glissante : la periode affichee reste stable
        info.periode_ms = (info.periode_ms <= 0) ? ecart : 0.8 * info.periode_ms + 0.2 * ecart;
    }
    info.dernier_ms = maintenant;
    const bool change = (info.nombre > 1 && texteDonnees != info.donnees);
    info.donnees = texteDonnees;
    if (change)
        info.changement_ms = maintenant;

    // Cherche la ligne de cet ID, ou l'endroit ou l'inserer pour garder le tri
    int ligne = 0;
    while (ligne < m_tableId->rowCount()
           && m_tableId->item(ligne, 0)->data(Qt::UserRole).toUInt() < id)
        ligne++;
    if (ligne == m_tableId->rowCount()
        || m_tableId->item(ligne, 0)->data(Qt::UserRole).toUInt() != id) {
        m_tableId->insertRow(ligne);
        for (int c = 0; c < 6; ++c)
            m_tableId->setItem(ligne, c, new QTableWidgetItem);
        m_tableId->item(ligne, 0)->setData(Qt::UserRole, id);
        m_tableId->item(ligne, 0)->setText(QString("0x%1").arg(id, 3, 16, QChar('0')).toUpper().replace("0X", "0x"));
    }

    m_tableId->item(ligne, 1)->setText(QString::number(data.size()));
    m_tableId->item(ligne, 2)->setText(texteDonnees);
    m_tableId->item(ligne, 3)->setText(decoder(id, data));
    m_tableId->item(ligne, 4)->setText(QString::number(info.nombre));
    m_tableId->item(ligne, 5)->setText(info.periode_ms > 0
                                           ? QString("%1 ms").arg(qRound(info.periode_ms))
                                           : QString("-"));
    if (change)
        m_tableId->item(ligne, 2)->setBackground(QColor("#FEF3C7"));
}

// Onglet "Trames (flux)" : toutes les trames, dans l'ordre d'arrivee.
void MainWindow::ajouterAuFlux(uint id, const QVector<quint8> &data)
{
    if (m_pause->isChecked())
        return;
    if (!m_filtreIds.isEmpty() && !m_filtreIds.contains(id))
        return;

    if (m_table->rowCount() >= NB_MAX_LIGNES)
        m_table->removeRow(0);

    QStringList octets;
    for (quint8 o : data)
        octets << QString("%1").arg(o, 2, 16, QChar('0')).toUpper();

    const int ligne = m_table->rowCount();
    m_table->insertRow(ligne);
    m_table->setItem(ligne, 0, new QTableWidgetItem(QString::number(m_chrono.elapsed() / 1000.0, 'f', 3)));
    m_table->setItem(ligne, 1, new QTableWidgetItem(QString("0x%1").arg(id, 3, 16, QChar('0')).toUpper().replace("0X", "0x")));
    m_table->setItem(ligne, 2, new QTableWidgetItem(QString::number(data.size())));
    m_table->setItem(ligne, 3, new QTableWidgetItem(octets.join(' ')));
    m_table->setItem(ligne, 4, new QTableWidgetItem(decoder(id, data)));
    m_table->scrollToBottom();
}

// Lit le champ "Filtre d'affichage" : liste d'ID hexadecimaux separes par , ; ou espaces.
void MainWindow::lireFiltre()
{
    m_filtreIds.clear();
    const QStringList morceaux = m_saisieFiltre->text().split(QRegularExpression("[\\s,;]+"), Qt::SkipEmptyParts);
    bool valide = true;
    for (const QString &m : morceaux) {
        bool ok = false;
        QString texte = m;
        if (texte.startsWith("0x", Qt::CaseInsensitive))
            texte = texte.mid(2);
        const uint id = texte.toUInt(&ok, 16);
        if (ok && id <= 0x7FF)
            m_filtreIds.insert(id);
        else
            valide = false;
    }
    m_saisieFiltre->setStyleSheet(valide ? QString() : QString("border: 1px solid #DC2626;"));
}

// Toutes les 500 ms : le bus est-il actif ? + fin du surlignage.
void MainWindow::verifierBus()
{
    const qint64 maintenant = m_port.isOpen() ? m_chrono.elapsed() : 0;

    if (!m_port.isOpen()) {
        m_etatBus->setText("● Pas de connexion");
        m_etatBus->setStyleSheet(QString("color: %1;").arg(COULEUR_SECONDAIRE));
    }
    else if (m_derniereTrame_ms >= 0 && maintenant - m_derniereTrame_ms < 1000) {
        m_etatBus->setText(QString("● Bus actif · %1 trames reçues").arg(m_nbTrames));
        m_etatBus->setStyleSheet("color: #16A34A; font-weight: 600;");
    }
    else {
        m_etatBus->setText("● Aucune trame reçue (F7 allumée ? câblage ?)");
        m_etatBus->setStyleSheet("color: #DC2626; font-weight: 600;");
    }

    // Retire le surlignage des donnees qui ont change il y a plus de 400 ms
    for (int ligne = 0; ligne < m_tableId->rowCount(); ++ligne) {
        const uint id = m_tableId->item(ligne, 0)->data(Qt::UserRole).toUInt();
        const InfoId &info = m_infos[id];
        if (info.changement_ms >= 0 && maintenant - info.changement_ms > 400)
            m_tableId->item(ligne, 2)->setBackground(QBrush());
    }
}

// ===========================================================================
//  Emission
// ===========================================================================

// Verifie la saisie manuelle puis envoie la trame.
void MainWindow::envoyerTrame()
{
    bool ok = false;
    QString texteId = m_saisieId->text().trimmed();
    if (texteId.startsWith("0x", Qt::CaseInsensitive))
        texteId = texteId.mid(2);
    const uint id = texteId.toUInt(&ok, 16);
    if (!ok || id > 0x7FF) {
        journal("ID invalide : entrez une valeur hexadécimale entre 000 et 7FF.");
        return;
    }

    const QStringList morceaux = m_saisieDonnees->text().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (morceaux.size() > 8) {
        journal("Trop de données : 8 octets maximum.");
        return;
    }

    QVector<uint> octets;
    for (const QString &texte : morceaux) {
        const uint valeur = texte.toUInt(&ok, 16);
        if (!ok || valeur > 0xFF) {
            journal("Octet invalide : \"" + texte + "\" (attendu : 00 à FF).");
            return;
        }
        octets.append(valeur);
    }
    envoyerCommande(id, octets);
}

// Construit la commande texte "T <id> <dlc> <octets>" et l'envoie a la F4.
void MainWindow::envoyerCommande(uint id, const QVector<uint> &octets)
{
    if (!m_port.isOpen()) {
        journal("Pas connecté : cliquez d'abord sur Connecter.");
        return;
    }

    QString commande = QString("T %1 %2").arg(id, 3, 16, QChar('0')).arg(octets.size());
    for (uint valeur : octets)
        commande += QString(" %1").arg(valeur, 2, 16, QChar('0'));
    commande = commande.toUpper();

    // Commande des phares : le voyant sera mis a jour quand la F4 repondra "OK"
    if (id == 0x500 && !octets.isEmpty())
        m_pharesEnAttente = (octets.first() != 0) ? 1 : 0;

    m_port.write((commande + "\r\n").toLatin1());
    journal("Envoi : " + commande);
}

// ===========================================================================
//  Divers
// ===========================================================================

void MainWindow::afficherPhares(int etat)
{
    QString couleur = "#CBD5E1";
    QString texte = "Inconnu";
    if (etat == 1) { couleur = COULEUR_PHARES; texte = "Allumés"; }
    if (etat == 0) { couleur = "#CBD5E1";      texte = "Éteints"; }
    m_voyantPhares->setStyleSheet(QString("background: %1; border-radius: 11px;").arg(couleur));
    m_textePhares->setText(texte);
}

void MainWindow::toutEffacer()
{
    m_table->setRowCount(0);
    m_tableId->setRowCount(0);
    m_infos.clear();
    m_nbTrames = 0;
    m_derniereTrame_ms = -1;
    m_courbeVitesse->effacer();
    m_courbeRegime->effacer();
    for (QLabel *l : {m_valVitesse, m_valRegime, m_valTemp, m_valBatterie, m_valCarburant})
        l->setText("--");
}

// Ajoute une ligne horodatee dans le journal.
void MainWindow::journal(const QString &texte)
{
    m_journal->appendPlainText(QTime::currentTime().toString("HH:mm:ss") + "  " + texte);
}

// Met a jour les boutons et l'indicateur selon l'etat de la connexion.
void MainWindow::mettreAJourEtat(bool connecte)
{
    m_boutonConnexion->setText(connecte ? "Déconnecter" : "Connecter");
    m_choixPort->setEnabled(!connecte);
    m_boutonRafraichir->setEnabled(!connecte);
    m_boutonEnvoyer->setEnabled(connecte);
    m_boutonPharesOn->setEnabled(connecte);
    m_boutonPharesOff->setEnabled(connecte);
    m_etat->setText(connecte ? "● Connecté" : "● Déconnecté");
    m_etat->setStyleSheet(connecte ? "color: #16A34A; font-weight: bold;" : QString("color: %1;").arg(COULEUR_SECONDAIRE));
}
