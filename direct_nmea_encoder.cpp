#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

// Direct AIS 6-bit encoder - minimal working version
class DirectAISEncoder {
public:
    static std::string encodeType5(int mmsi, const std::string& callsign, const std::string& name) {
        std::vector<std::string> fragments;

        // Build AIS bitstream for Type 5
        std::string bitstream = buildType5Bitstream(mmsi, callsign, name);

        // Convert to 6-bit encoded payload
        std::string payload = encodeTo6Bit(bitstream);

        // Split into NMEA fragments (max 61 chars per fragment for safety)
        return createNMEA(payload);
    }

private:
    static std::string buildType5Bitstream(int mmsi, const std::string& callsign, const std::string& name) {
        std::string bitstream;

        // Type: 5 (6 bits)
        bitstream += "000101";  // 5 in binary

        // Repeat indicator: 0 (2 bits)
        bitstream += "00";

        // MMSI: 374426000 (30 bits)
        bitstream += intToBinary(mmsi, 30);

        // AIS Version: 0 (2 bits)
        bitstream += "00";

        // Call Sign: 3EWC6 padded to 7 chars (42 bits)
        bitstream += encodeTextTo6Bit(callsign, 7);

        // Vessel Name: CRANE VESTA padded to 20 chars (120 bits)
        bitstream += encodeTextTo6Bit(name, 20);

        // Ship Type: 0 (8 bits)
        bitstream += "00000000";

        // Dimension: 0 (9 + 9 = 18 bits)
        bitstream += "000000000000000000";

        // Position ref + ETA: 0 (30 bits)
        bitstream += "000000000000000000000000000000";

        // Destination: empty (20 chars = 120 bits)
        bitstream += encodeTextTo6Bit("", 20);

        // DTE: 0 (1 bit)
        bitstream += "0";

        // Spare: 0 (6 bits)
        bitstream += "000000";

        return bitstream;
    }

    static std::string encodeTextTo6Bit(const std::string& text, int maxLen) {
        std::string result;
        std::string padded = text;

        // Pad to max length with spaces
        while (padded.length() < maxLen) {
            padded += ' ';
        }

        // Convert each character
        for (char ch : padded) {
            int val = 0;
            if (ch == ' ' || ch == '@') {
                val = 0;
            } else if (ch >= 'A' && ch <= 'Z') {
                val = ch - 'A' + 1;
            } else if (ch >= 'a' && ch <= 'z') {
                val = ch - 'a' + 1;
            } else if (ch >= '0' && ch <= '9') {
                val = ch - '0' + 48;
            }

            // Convert to 6-bit binary
            for (int i = 5; i >= 0; --i) {
                result += ((val >> i) & 1) ? '1' : '0';
            }
        }

        return result;
    }

    static std::string intToBinary(int value, int bits) {
        std::string result;
        for (int i = bits - 1; i >= 0; --i) {
            result += ((value >> i) & 1) ? '1' : '0';
        }
        return result;
    }

    static std::string encodeTo6Bit(const std::string& bitstream) {
        std::string result;

        for (size_t i = 0; i < bitstream.length(); i += 6) {
            if (i + 5 < bitstream.length()) {
                int val = 0;
                for (int j = 0; j < 6; ++j) {
                    if (bitstream[i + j] == '1') {
                        val += (1 << (5 - j));
                    }
                }
                // AIS 6-bit starts at ASCII 32
                char encoded = 32 + val;
                if (encoded >= 32 && encoded <= 126) {
                    result += encoded;
                } else {
                    result += '?';
                }
            }
        }

        return result;
    }

    static std::string createNMEA(const std::string& payload) {
        // For this simple case, we'll create single fragment if payload <= 61 chars
        if (payload.length() <= 61) {
            return "!AIVDM,1,1,,A," + payload + ",0*" + calculateChecksum("!AIVDM,1,1,,A," + payload + ",0");
        } else {
            // Split into 2 fragments
            std::string part1 = payload.substr(0, 61);
            std::string part2 = payload.substr(61);

            std::string nmea1 = "!AIVDM,2,1,,A," + part1 + ",0*" + calculateChecksum("!AIVDM,2,1,,A," + part1 + ",0");
            std::string nmea2 = "!AIVDM,2,2,,A," + part2 + ",0*" + calculateChecksum("!AIVDM,2,2,,A," + part2 + ",0");

            return nmea1 + "\n" + nmea2;
        }
    }

    static std::string calculateChecksum(const std::string& sentence) {
        int checksum = 0;
        for (size_t i = 1; i < sentence.length(); ++i) {
            checksum ^= (int)sentence[i];
        }

        std::stringstream ss;
        ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << checksum;
        return ss.str();
    }
};

int main() {
    std::cout << "=== CRANE VESTA DIRECT NMEA ENCODER ===" << std::endl << std::endl;

    // CRANE VESTA parameters
    int mmsi = 374426000;
    std::string callsign = "3EWC6";
    std::string vesselName = "CRANE VESTA";

    std::cout << "Input Parameters:" << std::endl;
    std::cout << "MMSI: " << mmsi << std::endl;
    std::cout << "Call Sign: '" << callsign << "'" << std::endl;
    std::cout << "Vessel Name: '" << vesselName << "'" << std::endl;
    std::cout << std::endl;

    // Generate NMEA
    std::string nmea = DirectAISEncoder::encodeType5(mmsi, callsign, vesselName);

    std::cout << "GENERATED NMEA:" << std::endl;
    std::cout << "================" << std::endl;
    std::cout << nmea << std::endl;
    std::cout << std::endl;

    // Extract payload for online decoder
    std::istringstream iss(nmea);
    std::string line;
    int lineNum = 1;

    std::cout << "PAYLOADS FOR ONLINE DECODER:" << std::endl;
    std::cout << "=============================" << std::endl;

    while (std::getline(iss, line)) {
        size_t start = line.find(",A,");
        size_t end = line.find(",", start + 3);

        if (start != std::string::npos && end != std::string::npos) {
            std::string payload = line.substr(start + 3, end - start - 3);
            std::cout << "Payload " << lineNum << ": " << payload << std::endl;
            lineNum++;
        }
    }

    std::cout << std::endl;
    std::cout << "EXPECTED DECODE RESULTS:" << std::endl;
    std::cout << "========================" << std::endl;
    std::cout << "Message Type: 5 (Static and Voyage Related Data)" << std::endl;
    std::cout << "MMSI: " << mmsi << std::endl;
    std::cout << "Call Sign: " << callsign << std::endl;
    std::cout << "Vessel Name: " << vesselName << std::endl;
    std::cout << "(Note: Space in vessel name will appear as @ in AIS decoder)" << std::endl;

    return 0;
}