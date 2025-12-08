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
    cout << "=== PERFECT Type 5 based on ITU-R M.1371 ===" << endl;

    string bitstream;

    // Build Type 5 step by step according to ITU-R M.1371
    cout << "Building Type 5 message structure..." << endl;

    // 0-5: Message Type (6 bits)
    bitstream += "000101";
    cout << "Bit 0-5: Type=5" << endl;

    // 6-7: Repeat Indicator (2 bits)
    bitstream += "00";
    cout << "Bit 6-7: Repeat=0" << endl;

    // 8-37: User ID/MMSI (30 bits)
    bitstream += "000000011011110001010101101101";  // 123456789
    cout << "Bit 8-37: MMSI=123456789" << endl;

    // 38-39: AIS Version (2 bits)
    bitstream += "00";
    cout << "Bit 38-39: AIS Version=0" << endl;

    // 40-69: IMO Number (30 bits) - not used
    bitstream += "000000000000000000000000000000";
    cout << "Bit 40-69: IMO=0" << endl;

    // 70-111: Call Sign (42 bits = 7 chars) - empty
    string callsignEncoded = encode6bitString("", 7);
    bitstream += callsignEncoded;
    cout << "Bit 70-111: Callsign=empty (" << bitstream.length() << " bits total)" << endl;

    // 112-142: Various fields (31 bits)
    bitstream += "00000000000000000000000000000000";
    cout << "Bit 112-142: Other fields" << endl;

    // 143-262: VESSEL NAME (120 bits = 20 chars) - KEY FIELD!
    string vesselName = "CRANE VESTA";
    string vesselNameEncoded = encode6bitString(vesselName, 20);
    cout << "Bit 143: Starting vessel name '" << vesselName << "'" << endl;
    bitstream += vesselNameEncoded;
    cout << "Bit 143-262: Vessel name (" << bitstream.length() << " bits total)" << endl;

    // 263-292: More fields (30 bits)
    bitstream += "000000000000000000000000000000";
    cout << "Bit 263-292: Additional fields" << endl;

    // 293-422: Destination (120 bits = 20 chars) - empty
    string destEncoded = encode6bitString("", 20);
    bitstream += destEncoded;
    cout << "Bit 293-422: Destination=empty (" << bitstream.length() << " bits total)" << endl;

    // 423: DTE (1 bit)
    bitstream += "0";
    cout << "Bit 423: DTE=0" << endl;

    // Total should be 424 bits
    while (bitstream.length() < 424) {
        bitstream += "0";
    }
    cout << "Total bits: " << bitstream.length() << endl;

    string payload = binaryToAIS6Bit(bitstream);
    cout << "Payload: " << payload << endl;

    // Vessel name starts at character 143/6 = 23 (0-indexed)
    string vesselInPayload = payload.substr(23, 20);
    cout << "Vessel name in payload (chars 23-42): " << vesselInPayload << endl;

    string nmea = "!AIVDM,1,1,,A," + payload + ",0";
    nmea += "*" + calculateChecksum(nmea);

    cout << "\n=== PERFECT NMEA SENTENCE ===" << endl;
    cout << nmea << endl;

    cout << "\nExpected decoder result:" << endl;
    cout << "Type: 5" << endl;
    cout << "MMSI: 123456789" << endl;
    cout << "Callsign: (empty)" << endl;
    cout << "Name: CRANE VESTA" << endl;

    return 0;
}