#include <iostream>
#include <string>
using namespace std;

// Implementation yang SAMA PERSIS dengan AIVDOEncoder.cpp
string binaryToAIS6Bit(const QString& bitstream) {
    int neededLength = ((bitstream.length() + 5) / 6) * 6;
    QString padded = bitstream.leftJustified(neededLength, '0');

    QString encoded;
    for (int i = 0; i < neededLength; i += 6) {
        QString chunk = padded.mid(i, 6);
        int value = chunk.toInt(nullptr, 2);
        value += 48;
        if (value > 87) value += 8;
        encoded += QChar(value);
    }
    return encoded;
}

string encode6bitString(const QString& text, int maxLen) {
    QString result;
    QString truncated = text.left(maxLen).toUpper();

    for (int i = 0; i < truncated.length(); ++i) {
        QChar ch = truncated.at(i);
        int val;

        if (ch == '@' || ch == ' ')
            val = 0;
        else if (ch >= 'A' && ch <= 'Z')
            val = ch.toLatin1() - 'A' + 1;
        else if (ch >= '0' && ch <= '9')
            val = ch.toLatin1() - '0' + 48;
        else
            val = 0;

        result += QString::number(val, 2).rightJustified(6, '0');
    }

    while (result.length() < maxLen * 6) {
        result += "000000";
    }

    return result;
}

// TEST working Type 5 yang sudah BENAR
void testKnownWorkingType5() {
    cout << "=== TEST WITH KNOWN WORKING Type 5 ===" << endl;

    // Test dengan Type 5 yang sudah terbukti bekerja
    string workingPayload = "55P5vT01<bG<4US`000H7TQP0B0000000000000G7U3@1@E<7000000000";
    string workingNMEA = "!AIVDM,1,1,,A," + workingPayload + ",0*2A";

    cout << "Known working Type 5: " << workingNMEA << endl;
    cout << "Length: " << workingPayload.length() << " chars" << endl;

    // Decode working payload untuk melihat vessel name
    string vesselNameChars = workingPayload.substr(14, 20);  // Karakter 14-33 = vessel name
    cout << "Vessel name chars: " << vesselNameChars << endl;
}

int main() {
    testKnownWorkingType5();
    return 0;
}