#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QElapsedTimer>
#include <QByteArray>
#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <QMap>
#include <QSet>

class QComboBox;
class QPushButton;
class QLabel;
class QTableWidget;
class QLineEdit;
class QPlainTextEdit;
class QCheckBox;
class QTimer;
class QHBoxLayout;

// ---------------------------------------------------------------------------
// Courbe : petit graphique "valeur en fonction du temps" (30 dernieres secondes).
// Dessine a la main avec QPainter : pas besoin du module Qt Charts.
// ---------------------------------------------------------------------------
class Courbe : public QWidget
{
public:
    Courbe(const QString &titre, const QString &unite, double maximum,
           const QColor &couleur, QWidget *parent = nullptr);
    void ajouterPoint(double temps_s, double valeur);
    void effacer();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_titre;
    QString m_unite;
    double m_max;
    QColor m_couleur;
    QVector<QPointF> m_points;              // (temps en secondes, valeur)
    static constexpr double FENETRE_S = 30.0;
};

// ---------------------------------------------------------------------------
// Fenetre principale du CAN Analyzer.
// Dialogue avec la STM32F4 par le port serie :
//   - lignes recues de la F4 : "R <id> <dlc> <octets...>" ou "# message"
//   - commandes envoyees a la F4 : "T <id> <dlc> <octets...>"
// ---------------------------------------------------------------------------
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void rafraichirPorts();
    void basculerConnexion();
    void lireDonnees();
    void envoyerTrame();
    void erreurPort(QSerialPort::SerialPortError erreur);
    void lireFiltre();
    void verifierBus();

private:
    // Statistiques d'un identifiant (vue "une ligne par ID")
    struct InfoId {
        int nombre = 0;            // nombre de trames recues
        qint64 dernier_ms = -1;    // date de la derniere trame
        double periode_ms = 0.0;   // periode moyenne mesuree
        qint64 changement_ms = -1; // date du dernier changement de donnees
        QString donnees;           // dernieres donnees (pour detecter un changement)
    };

    void traiterLigne(const QString &ligne);
    void traiterTrame(uint id, const QVector<quint8> &data);
    QString decoder(uint id, const QVector<quint8> &data) const;
    void ajouterAuFlux(uint id, const QVector<quint8> &data);
    void mettreAJourVueParId(uint id, const QVector<quint8> &data);
    void mettreAJourTableauDeBord(uint id, const QVector<quint8> &data);
    void envoyerCommande(uint id, const QVector<uint> &octets);
    void afficherPhares(int etat);   // 1 allumes, 0 eteints, -1 inconnu
    void toutEffacer();
    void journal(const QString &texte);
    void mettreAJourEtat(bool connecte);

    QWidget *creerTableauDeBord();
    QWidget *creerOngletFlux();
    QWidget *creerOngletParId();
    QLabel *creerCarte(QWidget *parent, QHBoxLayout *layout,
                       const QString &titre, const QString &unite);

    // Liaison serie avec la F4
    QSerialPort m_port;
    QByteArray m_tampon;      // caracteres recus en attente d'une fin de ligne
    QElapsedTimer m_chrono;   // temps ecoule depuis la connexion
    QTimer *m_minuteur = nullptr;

    // Etat
    QMap<uint, InfoId> m_infos;
    QSet<uint> m_filtreIds;   // vide = tout afficher
    int m_nbTrames = 0;
    qint64 m_derniereTrame_ms = -1;
    int m_pharesEnAttente = -1;

    // Connexion
    QComboBox *m_choixPort = nullptr;
    QPushButton *m_boutonRafraichir = nullptr;
    QPushButton *m_boutonConnexion = nullptr;
    QLabel *m_etat = nullptr;

    // Onglet 1 : tableau de bord
    QLabel *m_valVitesse = nullptr;
    QLabel *m_valRegime = nullptr;
    QLabel *m_valTemp = nullptr;
    QLabel *m_valBatterie = nullptr;
    QLabel *m_valCarburant = nullptr;
    Courbe *m_courbeVitesse = nullptr;
    Courbe *m_courbeRegime = nullptr;
    QLabel *m_voyantPhares = nullptr;
    QLabel *m_textePhares = nullptr;
    QPushButton *m_boutonPharesOn = nullptr;
    QPushButton *m_boutonPharesOff = nullptr;
    QLabel *m_etatBus = nullptr;

    // Onglet 2 : flux des trames
    QTableWidget *m_table = nullptr;
    QLineEdit *m_saisieFiltre = nullptr;
    QCheckBox *m_pause = nullptr;

    // Onglet 3 : une ligne par ID
    QTableWidget *m_tableId = nullptr;

    // Envoi manuel
    QLineEdit *m_saisieId = nullptr;
    QLineEdit *m_saisieDonnees = nullptr;
    QPushButton *m_boutonEnvoyer = nullptr;

    // Journal
    QPlainTextEdit *m_journal = nullptr;
};

#endif // MAINWINDOW_H
