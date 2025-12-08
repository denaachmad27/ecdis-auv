#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

// AIS 6-bit encoding implementation
std::string encode6bitToBinary(const std::string& text, int maxLen) {
    std::string result;
    std::string truncated = text.substr(0, maxLen);

    // Convert to uppercase
    for (char& c : truncated) {
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    }

    // Encode each character
    for (int i = 0; i < truncated.length(); ++i) {
        char ch = truncated[i];
        int val;

        if (ch == '@' || ch == ' ')
            val = 0;
        else if (ch >= 'A' && ch <= 'Z')
            val = ch - 'A' + 1;
        else if (ch >= '0' && ch <= '9')
            val = ch - '0' + 48;
        else
            val = 0;

        // Convert to 6-bit binary
        for (int j = 5; j >= 0; --j) {
            result += ((val >> j) & 1) ? '1' : '0';
        }
    }

    // Pad to exact length with zeros (@)
    while (result.length() < maxLen * 6) {
        result += "000000";
    }

    return result;
}

// Convert binary 6-bit to AIS payload
std::string binaryToAIS6Bit(const std::string& bitstream) {
    std::string result;

    for (size_t i = 0; i < bitstream.length(); i += 6) {
        if (i + 5 < bitstream.length()) {
            std::string sixBits = bitstream.substr(i, 6);
            int val = 0;

            for (int j = 0; j < 6; ++j) {
                if (sixBits[j] == '1') {
                    val += (1 << (5 - j));
                }
            }

            char aisChar = 32 + val;  // AIS 6-bit starts at ASCII 32
            if (aisChar >= 32 && aisChar <= 126) {  // Printable ASCII range
                result += aisChar;
            } else {
                result += '?';  // For non-printable chars
            }
        }
    }

    return result;
}

// Calculate NMEA checksum
std::string calculateChecksum(const std::string& sentence) {
    int checksum = 0;
    for (size_t i = 1; i < sentence.length(); ++i) {
        if (sentence[i] == '*') break;
        checksum ^= (int)sentence[i];
    }

    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << checksum;
    return ss.str();
}

// Split payload into NMEA fragments
std::vector<std::string> splitPayloadToVDM(const std::string& payload) {
    std::vector<std::string> result;
    int maxPayloadPerFragment = 62;
    int totalFragments = (payload.length() + maxPayloadPerFragment - 1) / maxPayloadPerFragment;

    for (int i = 0; i < totalFragments; ++i) {
        std::string fragmentPayload = payload.substr(i * maxPayloadPerFragment, maxPayloadPerFragment);

        std::stringstream ss;
        ss << "!AIVDM," << totalFragments << "," << (i + 1) << ",,A," << fragmentPayload << ",0*";

        std::string sentence = ss.str();
        sentence += calculateChecksum(sentence);
        result.push_back(sentence);
    }

    return result;
}

// Generate Type 5 AIS message
std::vector<std::string> generateType5(int mmsi, const std::string& callsign, const std::string& name) {
    std::string bitstream;

    // Type 5 (6 bits)
    int type = 5;
    for (int i = 5; i >= 0; --i) {
        bitstream += ((type >> i) & 1) ? '1' : '0';
    }

    // Repeat indicator (2 bits) = 0
    bitstream += "00";

    // MMSI (30 bits)
    for (int i = 29; i >= 0; --i) {
        bitstream += ((mmsi >> i) & 1) ? '1' : '0';
    }

    // AIS Version (2 bits) = 0
    bitstream += "00";

    // Call Sign (42 bits = 7 chars)
    bitstream += encode6bitToBinary(callsign, 7);

    // Vessel Name (120 bits = 20 chars)
    bitstream += encode6bitToBinary(name, 20);

    // Ship Type (8 bits) = 0
    bitstream += "00000000";

    // Dimension (18 bits) = 0
    bitstream += "000000000000000000";

    // Position reference + ETA (30 bits) = 0
    bitstream += "000000000000000000000000000000";

    // Destination (120 bits = 20 chars) = empty
    bitstream += encode6bitToBinary("", 20);

    // DTE (1 bit) = 0
    bitstream += "0";

    // Spare (6 bits) = 0
    bitstream += "000000";

    // Convert to payload
    std::string payload = binaryToAIS6Bit(bitstream);

    // Split into NMEA fragments
    return splitPayloadToVDM(payload);
}

int main() {
    std::cout << "=== CRANE VESTA NMEA GENERATOR ===" << std::endl;
    std::cout << std::endl;

    // CRANE VESTA data
    int mmsi = 374426000;
    std::string callsign = "3EWC6";
    std::string vesselName = "CRANE VESTA";

    std::cout << "Input Data:" << std::endl;
    std::cout << "MMSI: " << mmsi << std::endl;
    std::cout << "Call Sign: '" << callsign << "'" << std::endl;
    std::cout << "Vessel Name: '" << vesselName << "'" << std::endl;
    std::cout << std::endl;

    // Generate Type 5 AIS message
    std::vector<std::string> nmeaMessages = generateType5(mmsi, callsign, vesselName);

    std::cout << "Generated NMEA Messages:" << std::endl;
    for (size_t i = 0; i < nmeaMessages.size(); ++i) {
        std::cout << "NMEA " << (i + 1) << ": " << nmeaMessages[i] << std::endl;
    }

    std::cout << std::endl;

    // Extract payloads for easy copy-paste
    std::cout << "Payloads Only (for online decoder):" << std::endl;
    for (size_t i = 0; i < nmeaMessages.size(); ++i) {
        size_t start = nmeaMessages[i].find(",A,") + 3;
        size_t end = nmeaMessages[i].find(",", start);
        std::string payload = nmeaMessages[i].substr(start, end - start);
        std::cout << "Payload " << (i + 1) << ": " << payload << std::endl;
    }

    std::cout << std::endl;
    std::cout << "Expected Decode Results:" << std::endl;
    std::cout << "Type: 5 (Static and Voyage Related Data)" << std::endl;
    std::cout << "MMSI: " << mmsi << std::endl;
    std::cout << "Call Sign: " << callsign << std::endl;
    std::cout << "Vessel Name: " << vesselName << std::endl;

    return 0;
}