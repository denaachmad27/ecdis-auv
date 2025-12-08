#include <iostream>
#include <string>
using namespace std;

// Copy PERSIS encode6bitString dari AIVDOEncoder.cpp
string encode6bitString(const string& text, int maxLen) {
    string result;
    string truncated = text.substr(0, maxLen);

    // Uppercase
    for (char& c : truncated) {
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    }

    // Encode characters
    for (int i = 0; i < truncated.length(); ++i) {
        char ch = truncated[i];
        int val;

        if (ch == '@' || ch == ' ')
            val = 0;
        else if (ch >= 'A' && ch <= 'Z')
            val = ch - 'A' + 1;
        else if (ch >= '0' && ch <= '9')
            val = ch - '0' + 48;
        else
            val = 0;

        result += string(6, '0');
        for (int bit = 5; bit >= 0; --bit) {
            result[result.length() - 6 + (5 - bit)] = ((val >> bit) & 1) ? '1' : '0';
        }
    }

    // Pad to exactly maxLen * 6 bits
    while (result.length() < maxLen * 6) {
        result += "000000";
    }

    return result;
}

// Copy PERSIS binaryToAIS6Bit dari AIVDOEncoder.cpp
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
    cout << "=== EXACT Type 5 Implementation ===" << endl;

    // Test dengan "CRANE VESTA" menggunakan implementasi PERSIS AIVDOEncoder
    int mmsi = 123456789;
    string callsign = "";
    string name = "CRANE VESTA";
    int shipType = 0;
    double length = 0;
    double width = 0;
    string destination = "";

    cout << "MMSI: " << mmsi << endl;
    cout << "Callsign: '" << callsign << "'" << endl;
    cout << "Name: '" << name << "'" << endl;

    string bitstream;

    // Type 5: 6 bits
    bitstream += "000101";

    // Repeat: 2 bits
    bitstream += "00";

    // MMSI: 30 bits
    string mmsiBinary = "000000011011110001010101101101";  // 123456789
    bitstream += mmsiBinary;

    // AIS Version: 2 bits
    bitstream += "00";

    // Callsign: 7 chars = 42 bits
    string callsignEncoded = encode6bitString(callsign, 7);
    bitstream += callsignEncoded;

    // Vessel Name: 20 chars = 120 bits
    string nameEncoded = encode6bitString(name, 20);
    bitstream += nameEncoded;

    cout << "Name encoded to binary: " << nameEncoded << endl;

    // Ship Type: 8 bits
    bitstream += "00000000";

    // Length: 9 bits
    bitstream += "000000000";

    // Width: 9 bits
    bitstream += "000000000";

    // Position reference and ETA: 30 bits
    bitstream += "000000000000000000000000000000";

    // Destination: 20 chars = 120 bits
    string destEncoded = encode6bitString(destination, 20);
    bitstream += destEncoded;

    // DTE: 1 bit
    bitstream += "0";

    // Spare: 6 bits
    bitstream += "000000";

    cout << "Total bitstream length: " << bitstream.length() << " bits" << endl;

    string payload = binaryToAIS6Bit(bitstream);
    cout << "Payload: " << payload << endl;

    string nmea = "!AIVDM,1,1,,A," + payload + ",0";
    nmea += "*" + calculateChecksum(nmea);

    cout << "\n=== FINAL NMEA SENTENCE ===" << endl;
    cout << nmea << endl;

    return 0;
}