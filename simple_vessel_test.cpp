#include <iostream>
#include <string>
using namespace std;

// Test proper vessel name encoding following AIS 6-bit standard
void testVesselNameEncoding() {
    cout << "=== Testing Vessel Name Encoding ===" << endl;

    // Test cases
    string testNames[] = {
        "CRANE VESTA",
        "PATKAMLA SANTIAGO",
        "TEST",
        "A"
    };

    for (const string& originalName : testNames) {
        cout << "\nOriginal: '" << originalName << "' (length: " << originalName.length() << ")" << endl;

        // AIS 6-bit encoding for vessel name (max 20 chars for Type 5)
        string encoded;
        int maxLen = 20;

        // Encode each character to 6-bit AIS representation
        for (int i = 0; i < originalName.length() && i < maxLen; ++i) {
            char ch = originalName[i];
            int value = 0;

            if (ch >= 'A' && ch <= 'Z') {
                value = ch - 'A' + 1;  // A=1, B=2, ..., Z=26
            } else if (ch >= 'a' && ch <= 'z') {
                value = ch - 'a' + 1;  // a=1, b=2, ..., z=26
            } else if (ch >= '0' && ch <= '9') {
                value = ch - '0' + 48; // 0=48, 1=49, ..., 9=57
            } else if (ch == ' ') {
                value = 0;  // Space = 0
            } else {
                value = 0;  // Other chars = space (0)
            }

            // Convert to 6-bit binary
            for (int bit = 5; bit >= 0; --bit) {
                encoded += ((value >> bit) & 1) ? '1' : '0';
            }
        }

        // Pad remaining to exactly 20 chars (120 bits) with spaces (value=0)
        while (encoded.length() < maxLen * 6) {
            encoded += "000000";  // Space character padding
        }

        cout << "Encoded bits (" << encoded.length() << "): " << encoded << endl;

        // Decode back to test
        string decoded;
        for (size_t i = 0; i < encoded.length(); i += 6) {
            string sixbits = encoded.substr(i, 6);
            int value = 0;
            for (int j = 0; j < 6; ++j) {
                value = (value << 1) | (sixbits[j] - '0');
            }

            char ch;
            if (value >= 1 && value <= 26) {
                ch = 'A' + value - 1;
            } else if (value >= 30 && value <= 39) {
                ch = '0' + value - 30;
            } else {
                ch = ' ';  // value 0 or other = space
            }
            decoded += ch;
        }

        cout << "Decoded back: '" << decoded << "'" << endl;

        // Verify match
        string expected = originalName;
        expected.resize(20, ' ');  // Pad to 20 chars with spaces

        if (decoded == expected) {
            cout << "✓ ENCODING CORRECT" << endl;
        } else {
            cout << "✗ ENCODING FAILED" << endl;
            cout << "  Expected: '" << expected << "'" << endl;
            cout << "  Got:      '" << decoded << "'" << endl;
        }
    }
}

int main() {
    testVesselNameEncoding();

    cout << "\n=== PADDING STRATEGY TEST ===" << endl;
    cout << "For AIS Type 5 vessel names:" << endl;
    cout << "- Max length: 20 characters" << endl;
    cout << "- Encoding: 6-bit per character" << endl;
    cout << "- Padding: Spaces (value=0, binary=000000)" << endl;
    cout << "- NOT @ characters! @ is just for display when decoding value=0" << endl;

    return 0;
}