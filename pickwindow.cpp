// #include <QtGui>
#include <QtWidgets>
#include <QBoxLayout>

#include "pickwindow.h"
#include "ecwidget.h"
#include "nmeadecoder.h"

PickWindow::PickWindow(QWidget *parent, EcDictInfo *dict, EcDENC *dc)
: QDialog(parent)
{
  dictInfo = dict;
  denc = dc;

  setWindowTitle("Pick Report");

  setMinimumSize( QSize( 800, 600 ) );

  textEdit = new QTextEdit;
  textEdit->setReadOnly(true);  // Make it read-only
  textEdit->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);  // Allow text selection

  aisTemp = new QTextEdit;
  ownShipTemp = new QTextEdit;

  QVBoxLayout *mainLayout = new QVBoxLayout ();

  mainLayout->addWidget(textEdit);
  setLayout(mainLayout);
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
        if (navShip.speed_og != 0){
            //row = QString("Speed Over Ground: %1</b><br>").arg(navShip.speed_og);
            row = QString("<tr><td>SOG</td><td><b>%1 knots</b></td></tr>").arg(navShip.speed_og);
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
                            row = QString("<tr><td>COG</td><td><b>%1 kn</b></td></tr>").arg(QString(attrText));
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
                            row = QString("<tr><td>SOG</td><td><b>%1 °</b></td></tr>").arg(QString(attrText));
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
    ownShipTemp->setHtml(ownship);

  }
}

// QString PickWindow::ownShipAutoFill(QList<EcFeature> & pickFeatureList)
// {
//     EcFeature        feature;
//     EcClassToken     featToken;
//     char             featName[1024];
//     QString          row = "";
//     QString          text = "";
//     QString          ais = "";
//     QString          ownship = "";

//     QList<EcFeature>::Iterator iT;
//     for (iT=pickFeatureList.begin(); iT!=pickFeatureList.end(); ++iT)
//     {
//         feature = (*iT);

//         // Get the six character long feature token
//         EcFeatureGetClass(feature, dictInfo, featToken, sizeof(featToken));

//         // Translate the token to a human readable name
//         if (EcDictionaryTranslateObjectToken(dictInfo, featToken, featName, sizeof(featName)) != EC_DICT_OK)
//             strcpy(featName,"unknown");

//         row = QString("<br><b>%1 (%2)</b><br>").arg(QString(featName)).arg(QString(featToken));
//         text.append(row);

//         if (QString(featToken) == "ownshp"){
//             ownship.append(row);
//             ownship.append("<table width='100%' cellspacing='0' cellpadding='2'>");
//         }

//         if (featToken == QString("ownshp")){
//             if (navShip.lat != 0){
//                 row = QString("<tr><td>LAT</td><td><b>%1 °</b></td></tr>").arg(navShip.lat);
//                 text.append(row);
//                 ownship.append(row);
//             }
//             if (navShip.lon != 0){
//                 row = QString("<tr><td>LONG</td><td><b>%1 °</b></td></tr>").arg(navShip.lon);
//                 text.append(row);
//                 ownship.append(row);
//             }
//             if (navShip.depth != 0){
//                 row = QString("<tr><td>DEP</td><td><b>%1 m</b></td></tr>").arg(navShip.depth);
//                 text.append(row);
//                 ownship.append(row);
//             }
//             if (navShip.heading != 0){
//                 row = QString("<tr><td>HEAD</td><td><b>%1 °</b></td></tr>").arg(navShip.heading);
//                 text.append(row);
//                 ownship.append(row);
//             }
//             if (navShip.heading_og != 0){
//                 row = QString("<tr><td>HOG</td><td><b>%1 °</b></td></tr>").arg(navShip.heading_og);
//                 text.append(row);
//                 ownship.append(row);
//             }
//             if (navShip.speed != 0){
//                 row = QString("<tr><td>SPEED</td><td><b>%1 knots</b></td></tr>").arg(navShip.speed);
//                 text.append(row);
//                 ownship.append(row);
//             }
//             if (navShip.speed_og != 0){
//                 row = QString("<tr><td>SOG</td><td><b>%1 knots</b></td></tr>").arg(navShip.speed_og);
//                 text.append(row);
//                 ownship.append(row);
//             }
//             if (navShip.z != 0){
//                 row = QString("<tr><td>Z</td><td><b>%1 m</b></td></tr>").arg(navShip.z);
//                 text.append(row);
//                 ownship.append(row);
//             }
//         }

//         return ownship;
//     }

//     return "";
// }

QString PickWindow::ownShipAutoFill()
{
    QString          row = "";
    QString          text = "";
    QString          ownship = "";

    // row = QString("<br><b>OWNSHIP</b><br>");
    // text.append(row);

    ownship.append(row);
    ownship.append("<table width='100%' cellspacing='0' cellpadding='2'>");

    if (navShip.lat != 0){
        row = QString(
                  "<tr>"
                  "<td style='vertical-align:middle;'>LAT</td>"
                  "<td style='text-align:right;'>"
                  "<span style='font-size:12px; color:#71C9FF; font-weight:bold;'>%1</span>"
                  "</td>"
                  "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>°</td>"
                  "</tr>")
                  .arg(navShip.lat);
        text.append(row);
        ownship.append(row);
    }

    if (navShip.lon != 0){
        row = QString(
                  "<tr>"
                  "<td style='vertical-align:middle;'>LON</td>"
                  "<td style='text-align:right;'>"
                  "<span style='font-size:12px; color:#71C9FF; font-weight:bold;'>%1</span>"
                  "</td>"
                  "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>°</td>"
                  "</tr>")
                  .arg(navShip.lon);
        text.append(row);
        ownship.append(row);
    }

    if (navShip.heading != 0) {
        row = QString(
                  "<tr>"
                  "<td style='vertical-align:middle;'>HDG</td>"
                  "<td style='text-align:right;'>"
                  "<span style='font-size:20px; color:#71C9FF; font-weight:bold;'>%1</span>"
                  "</td>"
                  "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>°T</td>"
                  "</tr>")
                  .arg(navShip.heading);
        text.append(row);
        ownship.append(row);
    }

    if (navShip.rot != 0){
        row = QString(
                  "<tr>"
                  "<td style='vertical-align:middle;'>ROT</td>"
                  "<td style='text-align:right;'>"
                  "<span style='font-size:15px; color:#71C9FF; font-weight:bold;'>%1</span>"
                  "</td>"
                  "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>°/min</td>"
                  "</tr>")
                  .arg(navShip.rot);
        text.append(row);
        ownship.append(row);
    }

    if (navShip.heading_og != 0){
        row = QString(
                  "<tr>"
                  "<td style='vertical-align:middle;'>COG</td>"
                  "<td style='text-align:right;'>"
                  "<span style='font-size:18px; color:#71C9FF; font-weight:bold;'>%1</span>"
                  "</td>"
                  "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>°</td>"
                  "</tr>")
                  .arg(navShip.heading_og);
        text.append(row);
        ownship.append(row);
    }

    if (navShip.speed_og != 0){
        row = QString(
                  "<tr>"
                  "<td style='vertical-align:middle;'>SOG</td>"
                  "<td style='text-align:right;'>"
                  "<span style='font-size:15px; color:#71C9FF; font-weight:bold;'>%1</span>"
                  "</td>"
                  "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>knots</td>"
                  "</tr>")
                  .arg(navShip.speed_og);
        text.append(row);
        ownship.append(row);
    }

    if (navShip.depth_below_keel != 0){
        row = QString(
                  "<tr>"
                  "<td style='vertical-align:middle;'>DEP</td>"
                  "<td style='text-align:right;'>"
                  "<span style='font-size:15px; color:#71C9FF; font-weight:bold;'>%1</span>"
                  "</td>"
                  "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>m</td>"
                  "</tr>")
                  .arg(navShip.depth_below_keel);
        text.append(row);
        ownship.append(row);
    }

    if (navShip.speed != 0){
        row = QString(
                  "<tr>"
                  "<td style='vertical-align:middle;'>SPD</td>"
                  "<td style='text-align:right;'>"
                  "<span style='font-size:15px; color:#71C9FF; font-weight:bold;'>%1</span>"
                  "</td>"
                  "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>knots</td>"
                  "</tr>")
                  .arg(navShip.speed);
        text.append(row);
        ownship.append(row);
    }

    if (navShip.z != 0){
        row = QString(
                  "<tr>"
                  "<td style='vertical-align:middle;'>Z</td>"
                  "<td style='text-align:right;'>"
                  "<span style='font-size:15px; color:#71C9FF; font-weight:bold;'>%1</span>"
                  "</td>"
                  "<td style='vertical-align:middle; padding-left:7px; text-align:left;'>m</td>"
                  "</tr>")
                  .arg(navShip.z);
        text.append(row);
        ownship.append(row);
    }


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
