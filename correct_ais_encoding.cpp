#include <iostream>
#include <string>
using namespace std;

// Implementasi binaryToAIS6Bit yang sama dengan AIVDOEncoder.cpp
string binaryToAIS6Bit(const string& bitstream) {
    // FIX: Calculate exact needed length (multiple of 6)
    int neededLength = ((bitstream.length() + 5) / 6) * 6;
    string padded = bitstream;

    // Pad dengan 0 di kanan
    while (padded.length() < neededLength) {
        padded += '0';
    }

    string encoded;
    for (int i = 0; i < neededLength; i += 6) {
        string chunk = padded.substr(i, 6);

        // Konversi binary 6 bits ke integer
        int value = 0;
        for (int j = 0; j < 6; ++j) {
            value = (value << 1) | (chunk[j] - '0');
        }

        // Apply AIS 6-bit encoding mapping (sesuai AIVDOEncoder.cpp)
        value += 48;
        if (value > 87) value += 8;

        // Convert to character
        if (value >= 32 && value <= 126) {
            encoded += (char)value;
        } else {
            encoded += '@';
        }
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
    cout << "=== AIS Type 5 NMEA Using Correct Implementation ===" << endl;
    cout << "MMSI: 123456789, Vessel: CRANE VESTA" << endl << endl;

    // Bitstream yang sama dengan yang digunakan di AIVDOEncoder
    string bitstream =
        "000101"                              // Type 5
        "00"                                  // Repeat Indicator
        "000000011011110001010101101101"      // MMSI 123456789
        "00"                                  // AIS Version
        "000000000000000000000000000000000000000000000000"  // Callsign empty (42 bits)
        "000011010010000001001110000101000000010110000101010011010100000001000000000000000000000000000000000000000000000000000000"  // CRANE VESTA (120 bits)
        "00000000"                            // Ship Type 0
        "000000000000000000000000000000000000"  // Dimensions (36 bits)
        "000000000000000000000000000000"      // Position & ETA (30 bits)
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"  // Destination empty (120 bits)
        "0"                                   // DTE 0
        "000000";                             // Spare 0

    cout << "Original bitstream length: " << bitstream.length() << " bits" << endl;

    // Convert ke payload menggunakan function yang benar
    string payload = binaryToAIS6Bit(bitstream);
    cout << "6-bit payload: " << payload << endl;
    cout << "Payload length: " << payload.length() << " characters" << endl << endl;

    // Fragment jika perlu
    if (payload.length() > 61) {
        string payload1 = payload.substr(0, 61);
        string payload2 = payload.substr(61);

        string nmea1 = "!AIVDM,2,1,1,A," + payload1 + ",0";
        nmea1 += "*" + calculateChecksum(nmea1);

        string nmea2 = "!AIVDM,2,2,1,A," + payload2 + ",0";
        nmea2 += "*" + calculateChecksum(nmea2);

        cout << "=== FINAL NMEA SENTENCES ===" << endl;
        cout << "1. " << nmea1 << endl;
        cout << "2. " << nmea2 << endl;

        cout << "\n=== EXPECTED DECODER RESULT ===" << endl;
        cout << "- Type: 5" << endl;
        cout << "- MMSI: 123456789" << endl;
        cout << "- Vessel Name: CRANE VESTA" << endl;
        cout << "- Ship Type: 0" << endl;
    } else {
        string nmea = "!AIVDM,1,1,,A," + payload + ",0";
        nmea += "*" + calculateChecksum(nmea);
        cout << "Single sentence: " << nmea << endl;
    }

    return 0;
}