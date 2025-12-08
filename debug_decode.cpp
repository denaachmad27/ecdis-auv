#include <iostream>
#include <string>
#include <vector>
#include <sstream>
using namespace std;

// Function to convert 6-bit string to binary
string sixbitToBinary(const string& sixbit) {
    string binary;
    for (char c : sixbit) {
        int value = c - '0';
        binary += string(6, '0');
        for (int i = 5; i >= 0; --i) {
            binary[binary.length() - 6 + (5 - i)] = ((value >> i) & 1) ? '1' : '0';
        }
    }
    return binary;
}

// Function to decode 6-bit string to text
string decode6bitToString(const string& bits) {
    string result;
    for (size_t i = 0; i < bits.length(); i += 6) {
        if (i + 6 > bits.length()) break;

        string sixbits = bits.substr(i, 6);
        int value = stoi(sixbits, nullptr, 2);

        char ch;
        if (value >= 1 && value <= 26) {
            ch = 'A' + value - 1;
        } else if (value >= 30 && value <= 39) {
            ch = '0' + value - 30;
        } else if (value == 0) {
            ch = ' ';
        } else {
            ch = '@';
        }

        result += ch;
    }
    return result;
}

int main() {
    cout << "=== Testing Type 5 Decoding ===" << endl;

    // User's actual NMEA output from debug log
    string nmea1 = "!AIVDM,1,1,,A,55P5vT01<bG<4US`000H7TQP0B0000000000000G7U3@1@E<7000000000,0*2A";

    cout << "NMEA Line: " << nmea1 << endl;

    // Extract payload (between commas after channel 'A')
    size_t start = nmea1.find(",A,");
    size_t end = nmea1.find(",", start + 3);
    string payload = nmea1.substr(start + 3, end - start - 3);

    cout << "Payload: " << payload << endl;
    cout << "Payload length: " << payload.length() << endl;

    // Convert to binary
    string binary = sixbitToBinary(payload);
    cout << "Binary length: " << binary.length() << endl;

    // Debug: Show first 200 bits
    cout << "First 200 bits: " << binary.substr(0, 200) << endl;

    // Extract vessel name field (bits 82-201, 120 bits = 20 chars)
    if (binary.length() >= 201) {
        string nameBits = binary.substr(82, 120);
        cout << "Name field bits (82-201): " << nameBits << endl;
        cout << "Name field length: " << nameBits.length() << endl;

        string decodedName = decode6bitToString(nameBits);
        cout << "Decoded vessel name: '" << decodedName << "'" << endl;
    } else {
        cout << "ERROR: Not enough bits to extract vessel name!" << endl;
    }

    return 0;
}