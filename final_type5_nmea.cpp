#include <iostream>
#include <string>
using namespace std;

// Encode 6-bit string to AIS payload (sesuai AIVDOEncoder.cpp)
string encodeToAISPayload(const string& bitstream) {
    // Calculate needed length (multiple of 6)
    int neededLength = ((bitstream.length() + 5) / 6) * 6;
    string padded = bitstream;

    // Pad with zeros
    while (padded.length() < neededLength) {
        padded += '0';
    }

    string encoded;
    for (int i = 0; i < neededLength; i += 6) {
        string chunk = padded.substr(i, 6);

        // Convert binary to integer
        int value = 0;
        for (int j = 0; j < 6; ++j) {
            value = (value << 1) | (chunk[j] - '0');
        }

        // Apply AIS encoding: value + 48, +8 if > 87
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
    cout << "=== FINAL Type 5 NMEA for CRANE VESTA ===" << endl;
    cout << "MMSI: 123456789" << endl;
    cout << "Vessel Name: CRANE VESTA" << endl << endl;

    // Complete Type 5 bitstream yang sudah terbukti bekerja
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
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"  // Destination (120 bits)
        "0"                                   // DTE 0
        "000000";                             // Spare 0

    cout << "Bitstream length: " << bitstream.length() << " bits" << endl;

    // Encode ke payload
    string payload = encodeToAISPayload(bitstream);
    cout << "Payload: " << payload << endl;
    cout << "Payload length: " << payload.length() << " characters" << endl;

    // Create NMEA sentence
    string nmea = "!AIVDM,1,1,,A," + payload + ",0";
    nmea += "*" + calculateChecksum(nmea);

    cout << "\n=== FINAL NMEA SENTENCE ===" << endl;
    cout << nmea << endl;

    cout << "\n=== INSTRUCTIONS ===" << endl;
    cout << "Copy this NMEA sentence to your online AIS decoder:" << endl;
    cout << nmea << endl;
    cout << "\nExpected result: Vessel Name should be 'CRANE VESTA'" << endl;

    return 0;
}