// #include <QtGui>
#include <QtWidgets>
#include <QBoxLayout>

#include "pickwindow.h"
#include "ecwidget.h"
#include "nmeadecoder.h"
#include "appconfig.h"
#include "SettingsManager.h"

PickWindow::PickWindow(QWidget *parent, EcDictInfo *dict, EcDENC *dc)
: QDialog(parent)
{
  dictInfo = dict;
  denc = dc;

  setWindowTitle("Map Information");

  setMinimumSize( QSize( 400, 500 ) );

  textEdit = new QTextEdit;
  textEdit->setReadOnly(true);  // Make it read-only
  textEdit->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);  // Allow text selection

  aisTemp = new QTextEdit;
  ownShipTemp = new QTextEdit;

  QVBoxLayout *mainLayout = new QVBoxLayout ();

  mainLayout->addWidget(textEdit);
  setLayout(mainLayout);

  latViewMode = SettingsManager::instance().data().latViewMode;
  longViewMode = SettingsManager::instance().data().longViewMode;
}

QString PickWindow::buildAisHtml(EcFeature feature,
                       EcDictInfo* dictInfo,
                       double lat,
                       double lon,
                       double rangeNm,
                       double bearingDeg)
{
  EcFindInfo       fI;
  EcClassToken     featToken;
  EcAttributeToken attrToken;
  EcAttributeType  attrType;
  char             featName[1024];
  char             attrStr[1024];
  char             attrName[1024];
  char             attrText[1024];
  Bool             result;

  QString          ais;
  QString          row;

  // Get feature class token and name
  EcFeatureGetClass(feature, dictInfo, featToken, sizeof(featToken));
  if (EcDictionaryTranslateObjectToken(dictInfo, featToken, featName, sizeof(featName)) != EC_DICT_OK)
    strcpy(featName, "unknown");

  row = QString("<br><b>%1 (%2)</b><br>").arg(QString(featName)).arg(QString(featToken));
  ais.append(row);
  ais.append("<table width='100%' cellspacing='0' cellpadding='2'>");

  // Iterate attributes exactly like fill()
  result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_FIRST, attrStr, sizeof(attrStr));
  while (result) {
    strncpy(attrToken, attrStr, EC_LENATRCODE);
    attrToken[EC_LENATRCODE] = (char)0;

    if (EcDictionaryTranslateAttributeToken(dictInfo, attrToken, attrName, sizeof(attrName))) {
      if (EcDictionaryGetAttributeType(dictInfo, attrStr, &attrType) == EC_DICT_OK) {
        if (attrType == EC_ATTR_ENUM || attrType == EC_ATTR_LIST) {
          if (!EcDictionaryTranslateAttributeValue(dictInfo, attrStr, attrText, sizeof(attrText))) {
            attrText[0] = (char)0;
          }

          if (QString(attrToken) == "trksta") {
            row = QString("<tr><td>Track Status</td><td><b>%1</b></td></tr>").arg(attrText);
          } else if (QString(attrToken) == "actsta") {
            row = QString("<tr><td>Activation</td><td><b>%1</b></td></tr>").arg(attrText);
          } else if (QString(attrToken) == "posint") {
            row = QString("<tr><td>Pos Integrity</td><td><b>%1</b></td></tr>").arg(attrText);
          } else if (QString(attrToken) == "navsta") {
            row = QString("<tr><td>Nav Status</td><td><b>%1</b></td></tr>").arg(attrText);
          } else {
            row = QString("<tr><td>%1</td><td><b>%2</b></td></tr>").arg(QString(attrName)).arg(QString(attrText));
          }
        } else {
          strcpy(attrText, &attrStr[EC_LENATRCODE]);

          if (!strncmp(attrToken, "TXTDSC", 6) ||
              !strncmp(attrToken, "NTXTDS", 6) ||
              !strncmp(attrToken, "PICREP", 6) ||
              !strncmp(attrToken, "comctn", 6) ||
              !strncmp(attrToken, "schref", 6)) {
            // Keep consistent with fill(); skip file links here
            row = QString("");
          } else {
            if (QString(attrToken) == "cogcrs") {
              row = QString("<tr><td>COG</td><td><b>%1 °T</b></td></tr>").arg(QString(attrText));
            } else if (QString(attrToken) == "mmsino") {
              row = QString("<tr><td>MMSI</td><td><b>%1</b></td></tr>").arg(QString(attrText));
            } else if (QString(attrToken) == "roturn") {
              row = QString("<tr><td>ROT</td><td><b>%1 deg/min</b></td></tr>").arg(QString(attrText));
            } else if (QString(attrToken) == "headng") {
              row = QString("<tr><td>Heading</td><td><b>%1 °</b></td></tr>").arg(QString(attrText));
            } else if (QString(attrToken) == "sogspd") {
              row = QString("<tr><td>SOG</td><td><b>%1 kn</b></td></tr>").arg(QString(attrText));
            } else {
              row = QString("<tr><td>%1</td><td><b>%2</b></td></tr>").arg(QString(attrName)).arg(QString(attrText));
            }
          }
        }

        if (!row.isEmpty())
          ais.append(row);
      }
    }

    result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_NEXT, attrStr, sizeof(attrStr));
  }

  // Append additional navigation info: LAT, LONG, Range, Bearing (relative to ownship)
  ais.append(QString("<tr><td>Latitude</td><td><b>%1 °</b></td></tr>")
                 .arg(lat, 0, 'f', 6));
  ais.append(QString("<tr><td>Longitude</td><td><b>%1 °</b></td></tr>")
                 .arg(lon, 0, 'f', 6));
  ais.append(QString("<tr><td>Range</td><td><b>%1 NM</b></td></tr>")
                 .arg(rangeNm, 0, 'f', 2));
  ais.append(QString("<tr><td>Bearing</td><td><b>%1 °</b></td></tr>")
                 .arg(bearingDeg, 0, 'f', 1));

  ais.append("</table>");

  return ais;
}

void PickWindow::fill(QList<EcFeature> & pickFeatureList)
{
  EcFeature        feature;
  EcFindInfo       fI;
  EcClassToken     featToken;
  EcAttributeToken attrToken;
  EcAttributeType  attrType;
  char             featName[1024];
  char             attrStr[1024];
  char             attrName[1024];
  char             attrText[1024];
  Bool             result;
  QString          row = "";
  QString          text = "";
  QString          ais = "";
  QString          ownship = "";

  int              aisCount = 0;

  // It is advised to sort the features according to their geometric primitive type or feature class
  //1. all point features
  //2. all line features
  //3. all non meta area features
  //4. all meta feature (token starts with "M_"

  QList<EcFeature>::Iterator iT;
  for (iT=pickFeatureList.begin(); iT!=pickFeatureList.end(); ++iT)
  {
    feature = (*iT);

    // Get the six character long feature token
    EcFeatureGetClass(feature, dictInfo, featToken, sizeof(featToken));

    // Translate the token to a human readable name
    if (EcDictionaryTranslateObjectToken(dictInfo, featToken, featName, sizeof(featName)) != EC_DICT_OK)
      strcpy(featName,"unknown");

    row = QString("<br><b>%1 (%2)</b><br>").arg(QString(featName)).arg(QString(featToken));
    text.append(row);

    if (QString(featToken) == "aistar"){
        ais.append(row);
        ais.append("<table width='100%' cellspacing='0' cellpadding='2'>");

        // Increment aisCount
        aisCount++;

        // Limit the number of "aistar" to 3
        if (aisCount > 1) {
            break; // Stop processing additional "aistar" features
        }
    }
    if (QString(featToken) == "ownshp"){
        ownship.append(row);
        ownship.append("<table width='100%' cellspacing='0' cellpadding='2'>");
    }

    if (featToken == QString("ownshp")){
        if (navShip.lat != 0){
            //row = QString("Latitude: %1</b><br>").arg(navShip.lat);
            row = QString("<tr><td>LAT</td><td><b>%1 °</b></td></tr>").arg(navShip.lat);
            text.append(row);
            ownship.append(row);
        }
        if (navShip.lon != 0){
            //row = QString("Longitude: %1</b><br>").arg(navShip.lon);
            row = QString("<tr><td>LONG</td><td><b>%1 °</b></td></tr>").arg(navShip.lon);
            text.append(row);
            ownship.append(row);
        }
        if (navShip.heading != 0){
            //row = QString("Heading: %1</b><br>").arg(navShip.heading);
            row = QString("<tr><td>HDG</td><td><b>%1 °T</b></td></tr>").arg(navShip.heading);
            text.append(row);
            ownship.append(row);
        }
        if (navShip.rot != 0){
            //row = QString("Heading Over Ground: %1</b><br>").arg(navShip.heading_og);
            row = QString("<tr><td>ROT</td><td><b>%1 °/min</b></td></tr>").arg(navShip.rot);
            text.append(row);
            ownship.append(row);
        }
        if (navShip.heading_og != 0){
            //row = QString("Heading Over Ground: %1</b><br>").arg(navShip.heading_og);
            row = QString("<tr><td>COG</td><td><b>%1 °</b></td></tr>").arg(navShip.heading_og);
            text.append(row);
            ownship.append(row);
        }
        if (navShip.sog != 0){
            //row = QString("Speed Over Ground: %1</b><br>").arg(navShip.sog);
            row = QString("<tr><td>SOG</td><td><b>%1 knots</b></td></tr>").arg(navShip.sog);
            text.append(row);
            ownship.append(row);
        }

        if (navShip.depth != 0){
            //row = QString("Depth: %1</b><br>").arg(navShip.depth);
            row = QString("<tr><td>DEP</td><td><b>%1 m</b></td></tr>").arg(navShip.depth);
            text.append(row);
            ownship.append(row);
        }
        if (navShip.speed != 0){
            //row = QString("Speed: %1</b><br>").arg(navShip.speed);
            row = QString("<tr><td>SPEED</td><td><b>%1 knots</b></td></tr>").arg(navShip.speed);
            text.append(row);
            ownship.append(row);
        }
        if (navShip.z != 0){
            //row = QString("Z: %1</b><br>").arg(navShip.z);
            row = QString("<tr><td>Z</td><td><b>%1 m</b></td></tr>").arg(navShip.z);
            text.append(row);
            ownship.append(row);
        }
    }
    // Get the first attribute string (attribute token, value and delimiter) of the feature
    result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_FIRST, attrStr, sizeof(attrStr));

    QStringList tableRows;

    while (result){
        // Extract the six character long attribute token
        strncpy(attrToken, attrStr, EC_LENATRCODE);
        attrToken[EC_LENATRCODE]=(char)0;

        // Translate the token to a human readable name
        if (EcDictionaryTranslateAttributeToken(dictInfo, attrToken, attrName, sizeof(attrName))){
            // Get the attribute type (List, enumeration, integer, float, string, text)
            if (EcDictionaryGetAttributeType(dictInfo, attrStr, &attrType) == EC_DICT_OK){
                if (attrType == EC_ATTR_ENUM || attrType == EC_ATTR_LIST){
                    // translate the value to a human readable text
                    if(!EcDictionaryTranslateAttributeValue(dictInfo, attrStr, attrText, sizeof(attrText))){
                        attrText[0]=(char)0;
                    }

                    if (QString(attrToken) == "trksta"){
                        //row = QString("Track Status:  %1<br>").arg(QString(attrText));
                        row = QString("<tr><td>Track Status</td><td><b>%1</b></td></tr>").arg(attrText);
                    }
                    else if (QString(attrToken) == "actsta"){
                        //row = QString("Activation:  %1<br>").arg(QString(attrText));
                        row = QString("<tr><td>Activation</td><td><b>%1</b></td></tr>").arg(attrText);
                    }
                    else if (QString(attrToken) == "posint"){
                        //row = QString("Pos Integrity:  %1<br>").arg(QString(attrText));
                        row = QString("<tr><td>Pos Integrity</td><td><b>%1</b></td></tr>").arg(attrText);
                    }
                    else if (QString(attrToken) == "navsta"){
                        //row = QString("Nav Status:  %1<br>").arg(QString(attrText));
                        row = QString("<tr><td>Nav Status</td><td><b>%1</b></td></tr>").arg(attrText);
                    }
                    else {
                        //row = QString("  %1 (%2):  %3 (%4)<br>").arg(QString(attrName)).arg(QString(attrToken)).arg(QString(attrText)).arg(QString(&attrStr[EC_LENATRCODE]));
                        row = QString("<tr><td>%1</td><td><b>%2</b></td></tr>").arg(QString(attrName)).arg(QString(attrText));
                    }
                }
                else {
                    strcpy(attrText,&attrStr[EC_LENATRCODE]);
                    /* check for reference to external text or picture file */
                    if (!strncmp(attrToken, "TXTDSC", 6) ||
                        !strncmp(attrToken, "NTXTDS", 6) ||
                        !strncmp(attrToken, "PICREP", 6) ||
                        !strncmp(attrToken, "comctn", 6) || // InlandENC attribute
                        !strncmp(attrToken, "schref", 6))   // InlandENC attribute
                        {
                        char pathStr[512];
                        /* get the absolute path of the referenced file */
                        if (EcDENCGetPath(denc, attrText, pathStr, sizeof(pathStr), False)){
                            QDir dir(pathStr);
                            dir.cdUp();
                            QString linkStr = dir.filePath(attrText);
                            row  = QString("  %1 (%2): <a href=\"%3\">%4</a><br>").arg(QString(attrName)).arg(QString(attrToken)).arg(linkStr).arg(attrText);
                            /* open the file with the corresponding application */
                        }
                    }
                    else {
                        if (QString(attrToken) == "cogcrs"){
                            //row = QString("COG:  %1 kn<br>").arg(QString(attrText));
                            row = QString("<tr><td>COG</td><td><b>%1 °T</b></td></tr>").arg(QString(attrText));
                        }
                        else if (QString(attrToken) == "mmsino"){
                            //row = QString("MMSI:  %1<br>").arg(QString(attrText));
                            row = QString("<tr><td>MMSI</td><td><b>%1</b></td></tr>").arg(QString(attrText));
                        }
                        else if (QString(attrToken) == "roturn"){
                            //row = QString("ROT:  %1 deg/min<br>").arg(QString(attrText));
                            row = QString("<tr><td>ROT</td><td><b>%1 deg/min</b></td></tr>").arg(QString(attrText));
                        }
                        else if (QString(attrToken) == "headng"){
                            //row = QString("Heading:  %1 °<br>").arg(QString(attrText));
                            row = QString("<tr><td>Heading</td><td><b>%1 °</b></td></tr>").arg(QString(attrText));
                        }
                        else if (QString(attrToken) == "sogspd"){
                            //row = QString("SOG:  %1 °<br>").arg(QString(attrText));
                            row = QString("<tr><td>SOG</td><td><b>%1 kn</b></td></tr>").arg(QString(attrText));
                        }
                        else {
                            //row = QString("%1 (%2):  %3<br>").arg(QString(attrName)).arg(QString(attrToken)).arg(QString(attrText));
                            row = QString("<tr><td>%1</td><td><b>%2</b></td></tr>").arg(QString(attrName)).arg(QString(attrText));
                        }
                    }
                }
            }
        }

        text.append(QString(row));

        if (QString(featToken) == "aistar"){
            ais.append(QString(row));
        }
        if (QString(featToken) == "ownshp"){
            ownship.append(QString(row));
        }

        // Get the next attribute string
        result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_NEXT, attrStr, sizeof(attrStr));
    }

    if (QString(featToken) == "aistar") {
        ais.append("</table>");
    }

    textEdit->setHtml(text);
    aisTemp->setHtml(ais);
    //ownShipTemp->setHtml(ownship);

  }
}

void PickWindow::fillStyle(QList<EcFeature> & pickFeatureList, EcCoordinate lat, EcCoordinate lon)
{
    setWindowTitle("Map Information");

    EcFeature        feature;
    EcFindInfo       fI;
    EcClassToken     featToken;
    EcAttributeToken attrToken;
    EcAttributeType  attrType;
    char             featName[1024];
    char             attrStr[1024];
    char             attrName[1024];
    char             attrText[1024];
    Bool             result;

    QString text = "";

    QColor headerColor;

    if (AppConfig::isLight()) {
        headerColor = QColor(224, 224, 224); // abu terang (#e0e0e0)
    }
    else if (AppConfig::isDark()) {
        headerColor = QColor(20, 20, 20); // abu gelap
    }
    else {
        headerColor = QColor(20, 20, 40); // biru gelap
    }

    QString headerColorStr = headerColor.name();

    // koordinat
    text += "<div style='font-family:Arial; font-size:10pt; margin-bottom:8px;'>";
    text += QString(
                "<tr style='background-color:%1; font-weight:bold;'>"
                "<td colspan='2' style='padding:4px;'>LAT: %2 &nbsp;&nbsp; LON: %3</td>"
                "</tr>")
                .arg(headerColorStr)
                .arg(lat, 0, 'f', 6)
                .arg(lon, 0, 'f', 6);
    text += "</div>";

    // buka table
    text += "<div style='font-family:Arial; font-size:10pt;'>";
    text += "<table width='100%' cellspacing='0' cellpadding='6' "
            "style='border-collapse:collapse; font-size:10pt;'>";

    QMap<QString, int> featureCounters;

    for (auto iT = pickFeatureList.begin(); iT != pickFeatureList.end(); ++iT)
    {
        feature = (*iT);

        EcFeatureGetClass(feature, dictInfo, featToken, sizeof(featToken));
        if (EcDictionaryTranslateObjectToken(dictInfo, featToken, featName, sizeof(featName)) != EC_DICT_OK)
            strcpy(featName, "unknown");

        QString featTokenStr = QString(featToken);
        QString featNameStr  = QString(featName);

        int count = featureCounters.value(featTokenStr, 0) + 1;
        featureCounters[featTokenStr] = count;

        // header row
        text += QString(
                    "<tr style='background-color:%1; font-weight:bold;'>"
                    "<td colspan='2' style='padding:4px;'>[%2 %3]</td>"
                    "</tr>")
                    .arg(headerColorStr)
                    .arg(featNameStr.toUpper())
                    .arg(count);

        // loop attribute
        result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_FIRST, attrStr, sizeof(attrStr));
        while (result)
        {
            strncpy(attrToken, attrStr, EC_LENATRCODE);
            attrToken[EC_LENATRCODE] = (char)0;

            QString attrNameStr;
            QString attrValueStr;

            if (EcDictionaryTranslateAttributeToken(dictInfo, attrToken, attrName, sizeof(attrName))) {
                attrNameStr = QString(attrName);
                if (EcDictionaryGetAttributeType(dictInfo, attrStr, &attrType) == EC_DICT_OK) {
                    if (attrType == EC_ATTR_ENUM || attrType == EC_ATTR_LIST) {
                        if (!EcDictionaryTranslateAttributeValue(dictInfo, attrStr, attrText, sizeof(attrText)))
                            attrText[0] = (char)0;
                        attrValueStr = QString(attrText);
                    } else {
                        strcpy(attrText, &attrStr[EC_LENATRCODE]);
                        attrValueStr = QString(attrText);
                    }
                }
            }

            // formatting khusus Source date
            if (attrNameStr.compare("Source date", Qt::CaseInsensitive) == 0) {
                QDate date = QDate::fromString(attrValueStr, "yyyyMMdd");
                if (date.isValid()) {
                    attrValueStr = date.toString("dd-MM-yyyy");
                }
            }

            // formatting khusus Textual description (baca file isi TXT)
            if (attrNameStr.compare("Textual description", Qt::CaseInsensitive) == 0) {
                QString fileName = attrValueStr.trimmed();
                if (!fileName.isEmpty()) {
                    QString roamingPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
                    QDir dir(roamingPath);
                    dir.cdUp();

                    QString path = QString("%1/SevenCs/EC2007/DENC/TXT/%2/%3/%4/%5")
                    .arg(dir.path())
                        .arg(fileName.mid(0,2))
                        .arg(fileName.mid(2,2))
                        .arg(fileName.mid(4,2))
                        .arg(fileName);
                    QString parsed = parseTxtFile(path);
                    attrValueStr = parsed.isEmpty() ? QString("(tidak ada isi)") : parsed;
                }
            }

            // tambah row attribute
            text += QString(
                        "<tr style='border-bottom:1px solid #ddd;'>"
                        "<td style='width:40%; font-weight:bold; vertical-align:top;'>%1</td>"
                        "<td style='width:60%; vertical-align:top; white-space:pre-wrap;'>%2</td>"
                        "</tr>")
                        .arg(attrNameStr)
                        .arg(attrValueStr.toHtmlEscaped());

            result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_NEXT, attrStr, sizeof(attrStr));
        }
    }

    // tutup table
    text += "</table></div>";

    // tampilkan
    textEdit->setHtml(text);
}

void PickWindow::fillWarningOnly(QList<EcFeature> & pickFeatureList, EcCoordinate lat, EcCoordinate lon)
{
    setWindowTitle("Caution and Restricted Information");

    EcFeature        feature;
    EcFindInfo       fI;
    EcClassToken     featToken;
    EcAttributeToken attrToken;
    EcAttributeType  attrType;
    char             featName[1024];
    char             attrStr[1024];
    char             attrName[1024];
    char             attrText[1024];
    Bool             result;

    QString text = "";

    QColor headerColor;

    if (AppConfig::isLight()) {
        headerColor = QColor(224, 224, 224); // abu terang (#e0e0e0)
    }
    else if (AppConfig::isDark()) {
        headerColor = QColor(20, 20, 20); // abu gelap
    }
    else {
        headerColor = QColor(20, 20, 40); // biru gelap
    }

    QString headerColorStr = headerColor.name();

    // koordinat
    text += "<div style='font-family:Arial; font-size:10pt; margin-bottom:8px;'>"
            + QString("<tr style='background-color:%1; font-weight:bold;'>"
                      "<td colspan='2' style='padding:4px;'>LAT: %2 &nbsp;&nbsp; LON: %3</td>"
                      "</tr>")
                  .arg(headerColorStr)
                  .arg(lat, 0, 'f', 6)
                  .arg(lon, 0, 'f', 6)
            + "</div>";

    // buka table
    text += "<div style='font-family:Arial; font-size:10pt;'>"
            "<table width='100%' cellspacing='0' cellpadding='6' "
            "style='border-collapse:collapse; font-size:10pt;'>";

    QMap<QString, int> featureCounters;

    // daftar feature yang diizinkan
    static const QSet<QString> allowedFeatures = {
        "CTNARE", "RESARE", "FERYRT", "MIPARE", "OSPARE", "SPLARE", "SUBTLN"
    };

    for (auto iT = pickFeatureList.begin(); iT != pickFeatureList.end(); ++iT)
    {
        feature = (*iT);

        EcFeatureGetClass(feature, dictInfo, featToken, sizeof(featToken));
        if (EcDictionaryTranslateObjectToken(dictInfo, featToken, featName, sizeof(featName)) != EC_DICT_OK)
            strcpy(featName, "unknown");

        QString featTokenStr = QString(featToken);
        QString featNameStr  = QString(featName);

        // filter hanya feature tertentu
        if (!allowedFeatures.contains(featTokenStr)) {
            continue;
        }

        int count = featureCounters.value(featTokenStr, 0) + 1;
        featureCounters[featTokenStr] = count;

        // header row
        text += QString(
                    "<tr style='background-color:%1; font-weight:bold;'>"
                    "<td colspan='2' style='padding:4px;'>[%2 %3]</td>"
                    "</tr>")
                    .arg(headerColorStr)
                    .arg(featNameStr.toUpper())
                    .arg(count);

        // loop attribute
        result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_FIRST, attrStr, sizeof(attrStr));
        while (result)
        {
            strncpy(attrToken, attrStr, EC_LENATRCODE);
            attrToken[EC_LENATRCODE] = (char)0;

            QString attrNameStr;
            QString attrValueStr;

            if (EcDictionaryTranslateAttributeToken(dictInfo, attrToken, attrName, sizeof(attrName))) {
                attrNameStr = QString(attrName);
                if (EcDictionaryGetAttributeType(dictInfo, attrStr, &attrType) == EC_DICT_OK) {
                    if (attrType == EC_ATTR_ENUM || attrType == EC_ATTR_LIST) {
                        if (!EcDictionaryTranslateAttributeValue(dictInfo, attrStr, attrText, sizeof(attrText)))
                            attrText[0] = (char)0;
                        attrValueStr = QString(attrText);
                    } else {
                        strcpy(attrText, &attrStr[EC_LENATRCODE]);
                        attrValueStr = QString(attrText);
                    }
                }
            }

            if (attrNameStr.compare("Information in national language", Qt::CaseInsensitive) == 0) {
                result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_NEXT, attrStr, sizeof(attrStr));
                continue;
            }

            // formatting khusus Source date
            if (attrNameStr.compare("Source date", Qt::CaseInsensitive) == 0) {
                QDate date = QDate::fromString(attrValueStr, "yyyyMMdd");
                if (date.isValid()) {
                    attrValueStr = date.toString("dd-MM-yyyy");
                }
            }

            // formatting khusus Textual description (baca file isi TXT)
            if (attrNameStr.compare("Textual description", Qt::CaseInsensitive) == 0) {
                QString fileName = attrValueStr.trimmed();
                if (!fileName.isEmpty()) {
                    QString roamingPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
                    QDir dir(roamingPath);
                    dir.cdUp();

                    QString path = QString("%1/SevenCs/EC2007/DENC/TXT/%2/%3/%4/%5")
                    .arg(dir.path())
                        .arg(fileName.mid(0,2))
                        .arg(fileName.mid(2,2))
                        .arg(fileName.mid(4,2))
                        .arg(fileName);
                    QString parsed = parseTxtFile(path);
                    attrValueStr = parsed.isEmpty() ? QString("(tidak ada isi)") : parsed;
                }
            }

            // tambah row attribute
            text += QString(
                        "<tr style='border-bottom:1px solid #ddd;'>"
                        "<td style='width:40%; font-weight:bold; vertical-align:top;'>%1</td>"
                        "<td style='width:60%; vertical-align:top; white-space:pre-wrap;'>%2</td>"
                        "</tr>")
                        .arg(attrNameStr)
                        .arg(attrValueStr.toHtmlEscaped());

            result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_NEXT, attrStr, sizeof(attrStr));
        }
    }

    // tutup table
    text += "</table></div>";

    // tampilkan
    textEdit->setHtml(text);
}



// Tambahan helper untuk parsing isi TXT
QString PickWindow::parseTxtFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString("Tidak bisa membuka file: %1").arg(filePath);
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    // Pisah per baris
    QStringList lines = content.split(QRegExp("[\r\n]+"), Qt::SkipEmptyParts);

    QString currentTitle;
    QString currentBody;
    QList<QPair<QString, QString>> blocks;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();

        if (line.isEmpty()) continue;

        // Deteksi apakah line ALL CAPS = Judul
        if (line.toUpper() == line && line.length() > 2) {
            // simpan blok sebelumnya jika ada
            if (!currentTitle.isEmpty() || !currentBody.isEmpty()) {
                blocks.append(qMakePair(currentTitle, currentBody.trimmed()));
                currentBody.clear();
            }
            currentTitle = line;
        } else {
            // bagian isi
            currentBody += (currentBody.isEmpty() ? "" : " ") + line;
        }
    }

    // simpan blok terakhir
    if (!currentTitle.isEmpty() || !currentBody.isEmpty()) {
        blocks.append(qMakePair(currentTitle, currentBody.trimmed()));
    }

    // ambil hasil
    QString finalTitle;
    QString finalBody;
    if (!blocks.isEmpty()) {
        auto lastBlock = blocks.last();  // ambil terakhir
        finalTitle = lastBlock.first;
        finalBody  = lastBlock.second;
    }

    if (finalTitle.isEmpty() && finalBody.isEmpty()) {
        return QString();
    }

    // Format akhir
    QString result;
    if (!finalTitle.isEmpty())
        result += finalTitle + "\n";
    if (!finalBody.isEmpty())
        result += finalBody;

    return result.trimmed();
}


/*
QString PickWindow::ownShipAutoFill(QList<EcFeature> & pickFeatureList)
{
    EcFeature        feature;
    EcClassToken     featToken;
    char             featName[1024];
    QString          row = "";
    QString          text = "";
    QString          ais = "";
    QString          ownship = "";

    QList<EcFeature>::Iterator iT;
    for (iT=pickFeatureList.begin(); iT!=pickFeatureList.end(); ++iT)
    {
        feature = (*iT);

        // Get the six character long feature token
        EcFeatureGetClass(feature, dictInfo, featToken, sizeof(featToken));

        // Translate the token to a human readable name
        if (EcDictionaryTranslateObjectToken(dictInfo, featToken, featName, sizeof(featName)) != EC_DICT_OK)
            strcpy(featName,"unknown");

        row = QString("<br><b>%1 (%2)</b><br>").arg(QString(featName)).arg(QString(featToken));
        text.append(row);

        if (QString(featToken) == "ownshp"){
            ownship.append(row);
            ownship.append("<table width='100%' cellspacing='0' cellpadding='2'>");
        }

        if (featToken == QString("ownshp")){
            if (navShip.lat != 0){
                row = QString("<tr><td>LAT</td><td><b>%1 °</b></td></tr>").arg(navShip.lat);
                text.append(row);
                ownship.append(row);
            }
            if (navShip.lon != 0){
                row = QString("<tr><td>LONG</td><td><b>%1 °</b></td></tr>").arg(navShip.lon);
                text.append(row);
                ownship.append(row);
            }
            if (navShip.depth != 0){
                row = QString("<tr><td>DEP</td><td><b>%1 m</b></td></tr>").arg(navShip.depth);
                text.append(row);
                ownship.append(row);
            }
            if (navShip.heading != 0){
                row = QString("<tr><td>HEAD</td><td><b>%1 °</b></td></tr>").arg(navShip.heading);
                text.append(row);
                ownship.append(row);
            }
            if (navShip.heading_og != 0){
                row = QString("<tr><td>HOG</td><td><b>%1 °</b></td></tr>").arg(navShip.heading_og);
                text.append(row);
                ownship.append(row);
            }
            if (navShip.speed != 0){
                row = QString("<tr><td>SPEED</td><td><b>%1 knots</b></td></tr>").arg(navShip.speed);
                text.append(row);
                ownship.append(row);
            }
            if (navShip.sog != 0){
                row = QString("<tr><td>SOG</td><td><b>%1 knots</b></td></tr>").arg(navShip.sog);
                text.append(row);
                ownship.append(row);
            }
            if (navShip.z != 0){
                row = QString("<tr><td>Z</td><td><b>%1 m</b></td></tr>").arg(navShip.z);
                text.append(row);
                ownship.append(row);
            }
        }

        return ownship;
    }

    return "";
}
*/

QString PickWindow::ownShipAutoFill()
{
    QString          row = "";
    QString          text = "";
    QString          ownship = "";

    // row = QString("<br><b>OWNSHIP</b><br>");
    // text.append(row);

    QString sizing = QString(
                    "<tr>"
                    "<td style='vertical-align:middle; color:transparent; font-size:1px;'>-</td>"
                    "<td style='text-align:right;'>"
                    "<span style='font-size:1px; color:#71C9FF; font-weight:bold; color:transparent;'>"
                    "--------------------------------------------------------------"
                    "--------------------------------------------------------------"
                    "-------------------------------"
                    "</span>"
                    "</td>"
                    "<td style='vertical-align:middle; padding-left:7px; text-align:left; color:transparent; font-size:1px;'>-</td>"
                    "</tr>"
                    "</table>");

    auto toStringOrEmpty = [](double value) -> QString {
        if (std::isnan(value)) {
            return "N/A";
        }
        return QString::number(value);
    };

    auto isEmpty = [](QString value) -> QString {
        if (value.isEmpty()) {
            return "N/A";
        }
        return value;
    };

    ownship.append(row);

    // TABLE 1
    ownship.append("<table width='100%' cellspacing='0' cellpadding='2'>");

    // HDG
    row = QString(
              "<tr>"
              "<td style='vertical-align:middle;'>HDG</td>"
              "<td style='text-align:right;'>"
              "<span style='font-size:20px; color:#71C9FF; font-weight:bold;'>%1</span>"
              "</td>"
              "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>°T</td>"
              "</tr>")
              .arg(toStringOrEmpty(navShip.heading));
    text.append(row);
    ownship.append(row);

    // COG
    row = QString(
              "<tr>"
              "<td style='vertical-align:middle;'>COG</td>"
              "<td style='text-align:right;'>"
              "<span style='font-size:20px; color:#71C9FF; font-weight:bold;'>%1</span>"
              "</td>"
              "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>°T</td>"
              "</tr>")
              .arg(toStringOrEmpty(navShip.course_og));
    text.append(row);
    ownship.append(row);

    // SOG
    row = QString(
              "<tr>"
              "<td style='vertical-align:middle;'>SOG</td>"
              "<td style='text-align:right;'>"
              "<span style='font-size:20px; color:#71C9FF; font-weight:bold;'>%1</span>"
              "</td>"
              "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>kn</td>"
              "</tr>"
              )
              .arg(toStringOrEmpty(navShip.sog));
    text.append(row);
    ownship.append(row);

    ownship.append(sizing);
    ownship.append("<hr style='border:1px solid #ccc; margin:4px 0;'>");

    // TABLE 2
    ownship.append("<table width='100%' cellspacing='0' cellpadding='2'>");

    // LAT
    QString degree = "";
    QString latMode = SettingsManager::instance().data().latViewMode;
    if (latMode == "NAV_LAT"){
        navShip.slat = QString::number(navShip.lat);
        degree = "°";
    }
    else if (latMode == "NAV_LAT_DMS"){
        navShip.slat = navShip.lat_dms;
    }
    else {
        navShip.slat = navShip.lat_dmm;
    }

    int font_size = 0;
    if (navShip.slat.size() > 30) font_size = 8;
    else if (navShip.slat.size() >= 20) font_size = 10;
    else if (navShip.slat.size() < 20) font_size = 12;

    row = QString(
              "<tr>"
              "<td style='vertical-align:middle;'>LAT</td>"
              "<td style='text-align:right;'>"
              "<span style='font-size:%1px; color:#71C9FF; font-weight:bold;'>%2</span>"
              "</td>"
              "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>%3</td>"
              "</tr>")
              .arg(font_size).arg(isEmpty(navShip.slat)).arg(degree);
    text.append(row);
    ownship.append(row);

    // LON
    QString lonMode = SettingsManager::instance().data().longViewMode;
    degree = "";
    if (lonMode == "NAV_LONG"){
        navShip.slon = QString::number(navShip.lon);
        degree = "°";
    }
    else if (lonMode == "NAV_LONG_DMS"){
        navShip.slon = navShip.lon_dms;
    }
    else {
        navShip.slon = navShip.lon_dmm;
    }

    if (navShip.slon.size() > 30) font_size = 8;
    else if (navShip.slon.size() >= 20) font_size = 10;
    else if (navShip.slon.size() < 20) font_size = 12;

    row = QString(
              "<tr>"
              "<td style='vertical-align:middle;'>LON</td>"
              "<td style='text-align:right;'>"
              "<span style='font-size:%1px; color:#71C9FF; font-weight:bold;'>%2</span>"
              "</td>"
              "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>%3</td>"
              "</tr>")
              .arg(font_size).arg(isEmpty(navShip.slon)).arg(degree);
    text.append(row);
    ownship.append(row);

    // STW
    row = QString(
              "<tr>"
              "<td style='vertical-align:middle;'>LOG</td>"
              "<td style='text-align:right;'>"
              "<span style='font-size:15px; color:#71C9FF; font-weight:bold;'>%1</span>"
              "</td>"
              "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>kn</td>"
              "</tr>")
              .arg(toStringOrEmpty(navShip.stw));
    text.append(row);
    ownship.append(row);

    // DEPTH
    row = QString(
              "<tr>"
              "<td style='vertical-align:middle;'>DEPTH</td>"
              "<td style='text-align:right;'>"
              "<span style='font-size:15px; color:#71C9FF; font-weight:bold;'>%1</span>"
              "</td>"
              "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>m</td>"
              "</tr>")
              .arg(toStringOrEmpty(navShip.depth_below_keel));
    text.append(row);
    ownship.append(row);

    // DRAFT
    row = QString(
              "<tr>"
              "<td style='vertical-align:middle;'>DRAFT</td>"
              "<td style='text-align:right;'>"
              "<span style='font-size:15px; color:#71C9FF; font-weight:bold;'>%1</span>"
              "</td>"
              "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>m</td>"
              "</tr>")
              .arg(toStringOrEmpty(navShip.draft));
    text.append(row);
    ownship.append(row);

    // DRIFT
    row = QString(
              "<tr>"
              "<td style='vertical-align:middle;'>DRIFT</td>"
              "<td style='text-align:right;'>"
              "<span style='font-size:15px; color:#71C9FF; font-weight:bold;'>%1</span>"
              "</td>"
              "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>kn</td>"
              "</tr>")
              .arg(toStringOrEmpty(navShip.drift));
    text.append(row);
    ownship.append(row);

    // SET
    row = QString(
              "<tr>"
              "<td style='vertical-align:middle;'>SET</td>"
              "<td style='text-align:right;'>"
              "<span style='font-size:15px; color:#71C9FF; font-weight:bold;'>%1</span>"
              "</td>"
              "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>°T</td>"
              "</tr>")
              .arg(toStringOrEmpty(navShip.set));
    text.append(row);
    ownship.append(row);

    // ROT
    row = QString(
              "<tr>"
              "<td style='vertical-align:middle;'>RoT</td>"
              "<td style='text-align:right;'>"
              "<span style='font-size:15px; color:#71C9FF; font-weight:bold;'>%1</span>"
              "</td>"
              "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>°/min</td>"
              "</tr>")
              .arg(toStringOrEmpty(navShip.rot));
    text.append(row);
    ownship.append(row);

    ownship.append(sizing);
    ownship.append("<hr style='border:1px solid #ccc; margin:4px 0;'>");

    // TABLE 3
    ownship.append("<table width='100%' cellspacing='0' cellpadding='2'>");

    // WP Brg
    row = QString(
              "<tr>"
              "<td style='vertical-align:middle;'>WP BRG</td>"
              "<td style='text-align:right;'>"
              "<span style='font-size:12px; color:#71C9FF; font-weight:bold;'>%1</span>"
              "</td>"
              "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>°T</td>"
              "</tr>")
              .arg(toStringOrEmpty(activeRoute.rteWpBrg));
    text.append(row);
    ownship.append(row);

    // XTD
    row = QString(
              "<tr>"
              "<td style='vertical-align:middle;'>XTD</td>"
              "<td style='text-align:right;'>"
              "<span style='font-size:12px; color:#71C9FF; font-weight:bold;'>%1</span>"
              "</td>"
              "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>NM</td>"
              "</tr>")
              .arg(isEmpty(activeRoute.rteXtd));
    text.append(row);
    ownship.append(row);

    // CRS/CTM
    row = QString(
              "<tr>"
              "<td style='vertical-align:middle;'>CRS/CTM</td>"
              "<td style='text-align:right;'>"
              "<span style='font-size:12px; color:#71C9FF; font-weight:bold;'>%1 / %2</span>"
              "</td>"
              "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>°T</td>"
              "</tr>")
              .arg(toStringOrEmpty(activeRoute.rteCrs))
              .arg(toStringOrEmpty(activeRoute.rteCtm));
    text.append(row);
    ownship.append(row);

    // DTG
    row = QString(
              "<tr>"
              "<td style='vertical-align:middle;'>DTG</td>"
              "<td style='text-align:right;'>"
              "<span style='font-size:12px; color:#71C9FF; font-weight:bold;'>%1</span>"
              "</td>"
              "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>NM</td>"
              "</tr>")
              .arg(toStringOrEmpty(activeRoute.rteDtg));
    text.append(row);
    ownship.append(row);

    // TTG
    row = QString(
              "<tr>"
              "<td style='vertical-align:middle;'>TTG</td>"
              "<td style='text-align:right;'>"
              "<span style='font-size:12px; color:#71C9FF; font-weight:bold;'>%1</span>"
              "</td>"
              "<td style='vertical-align:middle; padding-left:7px; text-align:left;'></td>"
              "</tr>")
              .arg(isEmpty(activeRoute.rteTtg));
    text.append(row);
    ownship.append(row);

    // DEST ETA
    row = QString(
              "<tr>"
              "<td style='vertical-align:middle;'>DEST ETA</td>"
              "<td style='text-align:right;'>"
              "<span style='font-size:12px; color:#71C9FF; font-weight:bold;'>%1</span>"
              "</td>"
              "<td style='vertical-align:middle; padding-left:7px; text-align:left;'></td>"
              "</tr>")
              .arg(isEmpty(activeRoute.rteEta));
    text.append(row);
    ownship.append(row);

    // DEAD RECKON
    row = QString(
              "<tr>"
              "<td style='vertical-align:middle;'>DEAD RECKON</td>"
              "<td style='text-align:right;'>"
              "<span style='font-size:12px; color:#71C9FF; font-weight:bold;'>%1</span>"
              "</td>"
              "<td style='vertical-align:middle; padding-left:7px; text-align:left;'></td>"
              "</tr>")
              .arg(isEmpty(navShip.deadReckon));
    text.append(row);
    ownship.append(row);

    ownship.append(sizing);

    return ownship;
}

QJsonObject PickWindow::fillJson(QList<EcFeature> &pickFeatureList)
{
    EcFeature        feature;
    EcFindInfo       fI;
    EcClassToken     featToken;
    EcAttributeToken attrToken;
    EcAttributeType  attrType;
    char             featName[1024];
    char             attrStr[1024];
    char             attrName[1024];
    char             attrText[1024];
    Bool             result;
    QString          text = "";

    QJsonObject jsonOutput;

    // Loop through all features in the picked feature list
    QList<EcFeature>::Iterator iT;
    for (iT = pickFeatureList.begin(); iT != pickFeatureList.end(); ++iT)
    {
        feature = (*iT);

        // Get the six character long feature token
        EcFeatureGetClass(feature, dictInfo, featToken, sizeof(featToken));

        // Translate the token to a human readable name
        if (EcDictionaryTranslateObjectToken(dictInfo, featToken, featName, sizeof(featName)) != EC_DICT_OK)
            strcpy(featName, "unknown");

        QString featNameStr = QString(featName);

        // Create a QJsonObject for each feature
        QJsonObject featureObj;
        QJsonObject featureAttributes;

        // Get the first attribute string (attribute token, value and delimiter) of the feature
        result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_FIRST, attrStr, sizeof(attrStr));

        while (result)
        {
            // Extract the six character long attribute token
            strncpy(attrToken, attrStr, EC_LENATRCODE);
            attrToken[EC_LENATRCODE] = (char)0;

            // Translate the token to a human readable name
            if (EcDictionaryTranslateAttributeToken(dictInfo, attrToken, attrName, sizeof(attrName)))
            {
                // Get the attribute type (List, enumeration, integer, float, string, text)
                if (EcDictionaryGetAttributeType(dictInfo, attrStr, &attrType) == EC_DICT_OK)
                {
                    if (attrType == EC_ATTR_ENUM || attrType == EC_ATTR_LIST)
                    {
                        // Translate the value to a human readable text
                        if (!EcDictionaryTranslateAttributeValue(dictInfo, attrStr, attrText, sizeof(attrText)))
                            attrText[0] = (char)0;

                        featureAttributes[QString(attrName)] = QString("%1 (%2)").arg(QString(attrText)).arg(QString(&attrStr[EC_LENATRCODE]));
                    }
                    else
                    {
                        strcpy(attrText, &attrStr[EC_LENATRCODE]);

                        featureAttributes[QString(attrName)] = QString(attrText);
                    }
                }
            }

            // Get the next attribute string
            result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_NEXT, attrStr, sizeof(attrStr));
        }

        // Add the attributes to the main feature object
        featureObj["attributes"] = featureAttributes;

        // Add the feature object to the JSON output, with the feature name as the key
        jsonOutput[featNameStr] = featureObj;
    }
    QJsonDocument doc(jsonOutput);
    textEdit->setPlainText(doc.toJson(QJsonDocument::Indented));

    // Return the final JSON object containing all the features and their attributes
    return jsonOutput;
}

QJsonObject PickWindow::fillJsonSubs(QList<EcFeature> &pickFeatureList)
{
    EcFeature        feature;
    EcFindInfo       fI;
    EcClassToken     featToken;
    EcAttributeToken attrToken;
    EcAttributeType  attrType;
    char             featName[1024];
    char             attrStr[1024];
    char             attrName[1024];
    char             attrText[1024];
    Bool             result;
    QString          text = "";

    QJsonObject jsonOutput;

    // Loop through all features in the picked feature list
    QList<EcFeature>::Iterator iT;
    for (iT = pickFeatureList.begin(); iT != pickFeatureList.end(); ++iT)
    {
        feature = (*iT);

        // Get the six character long feature token
        EcFeatureGetClass(feature, dictInfo, featToken, sizeof(featToken));

        // Translate the token to a human readable name
        if (EcDictionaryTranslateObjectToken(dictInfo, featToken, featName, sizeof(featName)) != EC_DICT_OK)
            strcpy(featName, "unknown");

        QString featNameStr = QString(featName);

        // Create a QJsonObject for each feature
        QJsonObject featureObj;
        QJsonObject featureAttributes;

        // Get the first attribute string (attribute token, value and delimiter) of the feature
        result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_FIRST, attrStr, sizeof(attrStr));

        while (result)
        {
            // Extract the six character long attribute token
            strncpy(attrToken, attrStr, EC_LENATRCODE);
            attrToken[EC_LENATRCODE] = (char)0;

            // Translate the token to a human readable name
            if (EcDictionaryTranslateAttributeToken(dictInfo, attrToken, attrName, sizeof(attrName)))
            {
                // Get the attribute type (List, enumeration, integer, float, string, text)
                if (EcDictionaryGetAttributeType(dictInfo, attrStr, &attrType) == EC_DICT_OK)
                {
                    if (attrType == EC_ATTR_ENUM || attrType == EC_ATTR_LIST)
                    {
                        // Translate the value to a human readable text
                        if (!EcDictionaryTranslateAttributeValue(dictInfo, attrStr, attrText, sizeof(attrText)))
                            attrText[0] = (char)0;

                        featureAttributes[QString(attrName)] = QString("%1 (%2)").arg(QString(attrText)).arg(QString(&attrStr[EC_LENATRCODE]));
                    }
                    else
                    {
                        strcpy(attrText, &attrStr[EC_LENATRCODE]);

                        featureAttributes[QString(attrName)] = QString(attrText);
                    }
                }
            }

            // Get the next attribute string
            result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_NEXT, attrStr, sizeof(attrStr));
        }

        // Add the attributes to the main feature object
        featureObj["attributes"] = featureAttributes;


        //jsonOutput[featNameStr] = featureObj;

        // Add the feature object to the JSON output, with the feature name as the key
        if (!jsonOutput.contains(featNameStr)) {
            // Belum ada key ini, langsung masukkan object
            jsonOutput[featNameStr] = featureObj;
        } else {
            QJsonValue existingValue = jsonOutput.value(featNameStr);

            if (existingValue.isObject()) {
                // Ubah menjadi array karena sudah ada satu item sebelumnya
                QJsonArray array;
                array.append(existingValue.toObject()); // Tambahkan yang lama
                array.append(featureObj);               // Tambahkan yang baru
                jsonOutput[featNameStr] = array;
            } else if (existingValue.isArray()) {
                // Sudah array, tinggal append
                QJsonArray array = existingValue.toArray();
                array.append(featureObj);
                jsonOutput[featNameStr] = array;
            }
        }

    }

    return jsonOutput;
}


