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

        for (int bit = 5; bit >= 0; --bit) {
            result += ((val >> bit) & 1) ? '1' : '0';
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
    cout << "=== TESTING TYPE 5 WITH SIMPLE STRUCTURE ===" << endl;

    // Saya akan gunakan Type 5 struktur yang sama dengan working AIS libraries
    string bitstream;

    // Type 5 structure yang benar:
    // Type (6) + Repeat (2) + MMSI (30) + AIS Version (2) = 40 bits
    bitstream += "000101";  // Type 5
    bitstream += "00";      // Repeat
    bitstream += "000110111100000000010010111110111001";  // MMSI 7279981
    bitstream += "00";      // AIS Version

    // Skip semua field lain, fokus di vessel name
    // Pad ke posisi vessel name
    while (bitstream.length() < 111) {  // Skip ke posisi vessel name
        bitstream += "0";
    }

    // VESSEL NAME: "TESTTEST" (simple 8 chars)
    string vesselName = "TESTTEST";
    string vesselEncoded = encode6bitString(vesselName, 20);
    bitstream += vesselEncoded;

    // Pad to multiple of 6
    while (bitstream.length() % 6 != 0) {
        bitstream += "0";
    }

    cout << "Total bits: " << bitstream.length() << endl;
    cout << "Vessel name binary: " << vesselEncoded << endl;

    string payload = binaryToAIS6Bit(bitstream);
    string nmea = "!AIVDM,1,1,,A," + payload + ",0";
    nmea += "*" + calculateChecksum(nmea);

    cout << "NMEA: " << nmea << endl;
    cout << "Expected MMSI: 7279981" << endl;
    cout << "Expected Vessel: TESTTEST" << endl;

    return 0;
}