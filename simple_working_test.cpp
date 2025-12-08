#include <iostream>
#include <string>
using namespace std;

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

int main() {
    cout << "=== SIMPLE WORKING Type 5 ===" << endl;

    // Test dengan implementasi Type 5 yang SAMM dengan AIVDOEncoder.encodeType5()
    string bitstream;

    bitstream += "000101";  // Type 5 (6 bits)
    bitstream += "00";      // Repeat Indicator (2 bits)

    // MMSI 123456789
    bitstream += "000000011011110001010101101101";  // 30 bits

    // AIS Version 0
    bitstream += "00";      // 2 bits

    // Callsign "TEST123" (7 chars = 42 bits)
    // T=20=010100, E=5=000101, S=19=010011, T=20=010100
    // 1=1=000001, 2=2=000010, 3=3=000011
    bitstream += "010100000101010011010100000001000010000011000000";

    // Vessel Name "AIVDOTEST" (9 chars = 54 bits) + padding
    // A=1=000001, I=9=001001, V=22=010110, D=4=000100, O=15=001111
    // T=20=010100, E=5=000101, S=19=010011, T=20=010100
    bitstream += "000001001001010110000100001111010100000101010011010100";

    // Pad vessel name ke 20 chars (120 bits total)
    bitstream += "000000000000000000000000000000000000000000000000000000"; // 66 bits padding

    cout << "Bitstream length: " << bitstream.length() << endl;

    string payload = binaryToAIS6Bit(bitstream);
    cout << "Payload: " << payload << endl;
    cout << "Callsign (chars 13-19): " << payload.substr(13, 7) << endl;
    cout << "Vessel name (chars 27-46): " << payload.substr(27, 20) << endl;

    return 0;
}