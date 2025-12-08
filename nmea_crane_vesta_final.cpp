#include <iostream>
#include <string>
using namespace std;

// Standard AIS 6-bit character encoding table
string sixbitToAISPayload(const string& bitstream) {
    string payload;
    for (size_t i = 0; i < bitstream.length(); i += 6) {
        if (i + 6 > bitstream.length()) break;

        // Convert 6 bits to integer value
        int value = 0;
        for (int j = 0; j < 6; ++j) {
            if (bitstream[i + j] == '1') {
                value = (value << 1) | 1;
            } else {
                value = (value << 1) | 0;
            }
        }

        // AIS 6-bit to ASCII conversion (standard)
        if (value >= 0 && value <= 63) {
            if (value == 0) payload += '@';                // 0 = @
            else if (value >= 1 && value <= 26) payload += 'A' + value - 1;  // 1-26 = A-Z
            else if (value >= 27 && value <= 29) payload += '[' + (value - 27);  // 27-29 = [ \ ]
            else if (value >= 30 && value <= 39) payload += '0' + (value - 30);  // 30-39 = 0-9
            else if (value >= 40 && value <= 59) payload += 'A' + (value - 40);  // 40-59 = a-z
            else if (value == 60) payload += '`';          // 60 = `
            else payload += 'a' + (value - 61);           // 61-63 = a-c
        }
    }
    return payload;
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
    cout << "=== AIS Type 5 NMEA Output for CRANE VESTA ===" << endl;
    cout << "MMSI: 123456789" << endl;
    cout << "Vessel Name: CRANE VESTA" << endl << endl;

    // Complete Type 5 bitstream for CRANE VESTA
    string bitstream =
        "000101"                              // Type 5
        "00"                                  // Repeat Indicator
        "000000011011110001010101101101"      // MMSI 123456789
        "00"                                  // AIS Version
        "000000000000000000000000000000000000000000000000"  // Callsign (empty, 42 bits)
        "000011010010000001001110000101000000010110000101010011010100000001000000000000000000000000000000000000000000000000000000"  // Vessel Name: CRANE VESTA padded to 20 chars
        "00000000"                            // Ship Type (0)
        "000000000000000000000000000000000000"  // Dimensions A,B,C,D (36 bits)
        "000000000000000000000000000000"      // Position Reference & ETA (30 bits)
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"  // Destination (empty, 120 bits)
        "0"                                   // DTE
        "000000";                             // Spare

    cout << "Bitstream length: " << bitstream.length() << " bits" << endl;

    // Convert to 6-bit encoded payload
    string payload = sixbitToAISPayload(bitstream);
    cout << "Payload: " << payload << endl;
    cout << "Payload length: " << payload.length() << " characters" << endl << endl;

    // Check if fragmentation needed (NMEA max payload = 61 chars for multi-sentence)
    if (payload.length() <= 61) {
        string nmea = "!AIVDM,1,1,,A," + payload + ",0";
        nmea += "*" + calculateChecksum(nmea);

        cout << "=== SINGLE NMEA SENTENCE ===" << endl;
        cout << nmea << endl;
    } else {
        // Fragment into 2 sentences
        string payload1 = payload.substr(0, 61);
        string payload2 = payload.substr(61);

        string nmea1 = "!AIVDM,2,1,1,A," + payload1 + ",0";
        nmea1 += "*" + calculateChecksum(nmea1);

        string nmea2 = "!AIVDM,2,2,1,A," + payload2 + ",0";
        nmea2 += "*" + calculateChecksum(nmea2);

        cout << "=== DUAL NMEA SENTENCES ===" << endl;
        cout << "Sentence 1:" << endl;
        cout << nmea1 << endl << endl;
        cout << "Sentence 2:" << endl;
        cout << nmea2 << endl;

        cout << "\n=== FOR TESTING ===" << endl;
        cout << "You can test both sentences sequentially in an online AIS decoder." << endl;
        cout << "The decoder should show:" << endl;
        cout << "- Message Type: 5" << endl;
        cout << "- MMSI: 123456789" << endl;
        cout << "- Vessel Name: CRANE VESTA" << endl;
        cout << "- Ship Type: 0 (Unknown)" << endl;
        cout << "- Callsign: (empty)" << endl;
        cout << "- Destination: (empty)" << endl;
    }

    return 0;
}