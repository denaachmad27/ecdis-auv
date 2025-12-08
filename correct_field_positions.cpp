#include <iostream>
#include <string>
using namespace std;

string encode6bitString(const string& text, int maxLen) {
    string result;
    string truncated = text.substr(0, maxLen);
    for (char& c : truncated) {
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    }

    for (int i = 0; i < truncated.length(); ++i) {
        char ch = truncated[i];
        int val;
        if (ch == '@' || ch == ' ') val = 0;
        else if (ch >= 'A' && ch <= 'Z') val = ch - 'A' + 1;
        else if (ch >= '0' && ch <= '9') val = ch - '0' + 48;
        else val = 0;

        result += string(6, '0');
        for (int bit = 5; bit >= 0; --bit) {
            result[result.length() - 6 + (5 - bit)] = ((val >> bit) & 1) ? '1' : '0';
        }
    }

    while (result.length() < maxLen * 6) {
        result += "000000";
    }
    return result;
}

string binaryToAIS6Bit(const string& bitstream) {
    int neededLength = ((bitstream.length() + 5) / 6) * 6;
    string padded = bitstream;
    while (padded.length() < neededLength) {
        padded += '0';
    }

    string encoded;
    for (int i = 0; i < neededLength; i += 6) {
        string chunk = padded.substr(i, 6);
        int value = 0;
        for (int j = 0; j < 6; ++j) {
            value = (value << 1) | (chunk[j] - '0');
        }

        value += 48;
        if (value > 87) value += 8;
        encoded += (char)value;
    }
    return encoded;
}

string calculateChecksum(const string& sentence) {
    unsigned char checksum = 0;
    for (size_t i = 1; i < sentence.length(); ++i) {
        if (sentence[i] == '*') break;
        checksum ^= (unsigned char)sentence[i];
    }
    char hex[3];
    sprintf(hex, "%02X", checksum);
    return string(hex);
}

int main() {
    cout << "=== FIXING FIELD POSITIONS ===" << endl;

    // BERDASARKAN DECODER RESULT:
    // Callsign menunjukkan "@@CRANE" = vessel name
    // Name menunjukkan "@VESTA" = callsign
    // BERARTI POSISI FIELD TERBALIK!

    string bitstream;

    bitstream += "000101";  // Type 5
    bitstream += "00";      // Repeat
    bitstream += "000000011011110001010101101101";  // MMSI 123456789
    bitstream += "00";      // AIS Version

    // CALLSIGN (7 chars = 42 bits): KITA ISI DENGAN SEBAGIAN VESSEL NAME
    // "@@CRANE" = 7 chars
    string partialVessel = "@@CRANE";  // 7 chars
    string callsignEncoded = encode6bitString(partialVessel, 7);
    bitstream += callsignEncoded;

    // VESSEL NAME (20 chars = 120 bits): "@VESTA" + padding
    string vesselName = "@VESTA";  // 6 chars
    string nameEncoded = encode6bitString(vesselName, 20);
    bitstream += nameEncoded;

    // Rest of fields
    bitstream += "00000000";  // Ship type
    bitstream += string(9, '0');  // Length
    bitstream += string(9, '0');  // Width
    bitstream += string(30, '0');  // Position & ETA
    bitstream += encode6bitString("", 20);  // Destination
    bitstream += "0";  // DTE
    bitstream += "000000";  // Spare

    cout << "Total bits: " << bitstream.length() << endl;

    string payload = binaryToAIS6Bit(bitstream);
    string nmea = "!AIVDM,1,1,,A," + payload + ",0";
    nmea += "*" + calculateChecksum(nmea);

    cout << "\n=== TEST NMEA ===" << endl;
    cout << nmea << endl;

    cout << "\nExpected decoder result:" << endl;
    cout << "Callsign: @@CRANE" << endl;
    cout << "Name: @VESTA (with padding)" << endl;

    return 0;
}