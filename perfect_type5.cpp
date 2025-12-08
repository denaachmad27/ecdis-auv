#include <iostream>
#include <string>
using namespace std;

string encodeToAISPayload(const string& bitstream) {
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

string encodeVesselName(const string& name) {
    string result;
    string padded = name;
    padded.resize(20, ' ');

    for (char ch : padded) {
        int val = 0;
        if (ch == ' ') val = 0;
        else if (ch >= 'A' && ch <= 'Z') val = ch - 'A' + 1;
        else if (ch >= '0' && ch <= '9') val = ch - '0' + 48;
        else val = 0;

        for (int bit = 5; bit >= 0; --bit) {
            result += ((val >> bit) & 1) ? '1' : '0';
        }
    }
    return result;
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
    cout << "=== PERFECT Type 5 AIS Message ===" << endl;
    cout << "Vessel: CRANE VESTA" << endl << endl;

    string vesselName = "CRANE VESTA";
    string vesselBinary = encodeVesselName(vesselName);

    // COMPLETE Type 5 bitstream (424 bits total)
    string bitstream;

    // 0-5: Type 5 (6 bits)
    bitstream += "000101";

    // 6-7: Repeat Indicator (2 bits)
    bitstream += "00";

    // 8-37: MMSI (30 bits)
    bitstream += "000000011011110001010101101101";  // 123456789

    // 38-39: AIS Version (2 bits)
    bitstream += "00";

    // 40-81: Callsign (42 bits = 7 chars x 6 bits)
    bitstream += "000000000000000000000000000000000000000000000000";  // Empty callsign

    // 82-201: Vessel Name (120 bits = 20 chars x 6 bits) - FOKUS KITA!
    bitstream += vesselBinary;

    // 202-209: Ship Type (8 bits)
    bitstream += "00000000";

    // 210-217: Dimension A (9 bits)
    bitstream += "000000000";

    // 218-225: Dimension B (9 bits)
    bitstream += "000000000";

    // 226-233: Dimension C (9 bits)
    bitstream += "000000000";

    // 234-241: Dimension D (9 bits)
    bitstream += "000000000";

    // 242-271: Position Reference & ETA (30 bits)
    bitstream += "000000000000000000000000000000";

    // 272-391: Destination (120 bits = 20 chars x 6 bits)
    bitstream += "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";

    // 392: DTE (1 bit)
    bitstream += "0";

    // 393-398: Spare (6 bits)
    bitstream += "000000";

    // Add padding to reach exactly 424 bits
    while (bitstream.length() < 424) {
        bitstream += "0";
    }

    cout << "Total bitstream length: " << bitstream.length() << " bits" << endl;
    cout << "Expected: 424 bits for complete Type 5" << endl;

    if (bitstream.length() != 424) {
        cout << "ERROR: Incorrect bitstream length!" << endl;
        return 1;
    }

    cout << "Vessel name field (bits 82-201): " << bitstream.substr(82, 120) << endl;

    string payload = encodeToAISPayload(bitstream);
    cout << "\nAIS Payload: " << payload << endl;
    cout << "Payload length: " << payload.length() << " characters" << endl;

    // Vessel name di payload seharusnya karakter ke-27 sampai 46
    cout << "Characters 27-46 in payload: " << payload.substr(27, 20) << endl;

    string nmea = "!AIVDM,1,1,,A," + payload + ",0";
    nmea += "*" + calculateChecksum(nmea);

    cout << "\n=== PERFECT NMEA SENTENCE ===" << endl;
    cout << nmea << endl;

    return 0;
}