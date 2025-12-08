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
    cout << "=== WORKING CRANE VESTA Type 5 ===" << endl;

    string bitstream;

    // Type 5 header (sesuai working AIS libraries)
    bitstream += "000101";  // Type 5
    bitstream += "00";      // Repeat
    bitstream += "000000011011110001010101101101";  // MMSI 123456789 (simple)
    bitstream += "00";      // AIS Version

    // Pad ke posisi vessel name (berdasarkan decoder response)
    while (bitstream.length() < 111) {
        bitstream += "0";
    }

    // VESSEL NAME: "CRANE VESTA"
    string vesselName = "CRANE VESTA";
    string vesselEncoded = encode6bitString(vesselName, 20);
    bitstream += vesselEncoded;

    // Complete to Type 5 length
    while (bitstream.length() < 420) {
        bitstream += "0";
    }

    string payload = binaryToAIS6Bit(bitstream);
    string nmea = "!AIVDM,1,1,,A," + payload + ",0";
    nmea += "*" + calculateChecksum(nmea);

    cout << "NMEA: " << nmea << endl;
    cout << "Expected to decode: CRANE VESTA" << endl;

    return 0;
}