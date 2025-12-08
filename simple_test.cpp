#include <iostream>
#include <string>
#include <vector>

using namespace std;

string encode6bitString(const string &text, int maxLen) {
    string encoded;

    cout << "DEBUG encode6bitString: Input='" << text << "', maxLen=" << maxLen << endl;

    for (int i = 0; i < text.length() && i < maxLen; ++i) {
        char ch = text[i];
        int value = 0;

        cout << "DEBUG: Processing char '" << ch << "' (ASCII " << (int)ch << ")" << endl;

        if (ch >= 'A' && ch <= 'Z') {
            value = ch - 'A' + 1;
            cout << "DEBUG: Uppercase, value=" << value << endl;
        } else if (ch >= 'a' && ch <= 'z') {
            value = ch - 'a' + 1;
            cout << "DEBUG: Lowercase, value=" << value << endl;
        } else if (ch >= '0' && ch <= '9') {
            value = ch - '0' + 30;
            cout << "DEBUG: Digit, value=" << value << endl;
        } else if (ch == ' ') {
            value = 0;
            cout << "DEBUG: Space, value=" << value << endl;
        } else {
            // Map punctuation according to AIS 6-bit table
            switch (ch) {
                case '!': value = 37; break;
                case '"': value = 38; break;
                case '#': value = 39; break;
                case '$': value = 40; break;
                case '%': value = 41; break;
                case '&': value = 42; break;
                case '\'': value = 43; break;
                case '(': value = 44; break;
                case ')': value = 45; break;
                case '*': value = 46; break;
                case '+': value = 47; break;
                case ',': value = 48; break;
                case '-': value = 49; break;
                case '.': value = 50; break;
                case '/': value = 51; break;
                case ':': value = 52; break;
                case ';': value = 53; break;
                case '<': value = 54; break;
                case '=': value = 55; break;
                case '>': value = 56; break;
                case '?': value = 57; break;
                case '@': value = 58; break;
                case '[': value = 59; break;
                case '\\': value = 60; break;
                case ']': value = 61; break;
                case '^': value = 62; break;
                case '_': value = 63; break;
                default: value = 0; cout << "DEBUG: Unmapped char, using space value 0" << endl; break;
            }
        }

        encoded += char('0' + value);
        cout << "DEBUG: Encoded char: '" << char('0' + value) << "' (value " << value << ")" << endl;
    }

    // Pad to maxLen if needed
    while (encoded.length() < maxLen) {
        encoded += '0';  // space character
    }

    cout << "DEBUG: Final encoded string: '" << encoded << "' (length " << encoded.length() << ")" << endl;
    return encoded;
}

int main() {
    cout << "=== Testing 6-bit Encoding ===" << endl;

    string vesselName = "CRANE VESTA";
    cout << "Original: " << vesselName << endl;

    string encoded = encode6bitString(vesselName, 20);
    cout << "Encoded: " << encoded << endl;

    return 0;
}