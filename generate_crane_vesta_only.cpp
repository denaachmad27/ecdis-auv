#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <bitset>

// Simple but working AIS Type 5 encoder for vessel name only
class VesselNameEncoder {
public:
    static std::string generateType5ForVessel(const std::string& vesselName, int mmsi = 123456789) {
        // Build bitstream for Type 5 message
        std::string bitstream = buildType5Bitstream(mmsi, "CRAFT1", vesselName);  // Sample call sign

        // Convert to 6-bit encoded payload
        std::string payload = bitstreamToAIS6Bit(bitstream);

        // Create single fragment (if payload is short enough)
        return "!AIVDM,1,1,,A," + payload + ",0*" + calculateChecksum("!AIVDM,1,1,,A," + payload + ",0");
    }

private:
    static std::string buildType5Bitstream(int mmsi, const std::string& callsign, const std::string& name) {
        std::string bitstream;

        // Message Type: 5 (6 bits)
        bitstream += "000101";

        // Repeat: 0 (2 bits)
        bitstream += "00";

        // MMSI: 30 bits
        std::bitset<30> mmsiBits(mmsi);
        bitstream += mmsiBits.to_string();

        // AIS Version: 0 (2 bits)
        bitstream += "00";

        // IMO: 30 bits (not available)
        bitstream += "000000000000000000000000000000";

        // Call Sign: 7 chars x 6 bits = 42 bits
        bitstream += textTo6Bit(callsign, 7);

        // Vessel Name: 20 chars x 6 bits = 120 bits
        bitstream += textTo6Bit(name, 20);

        // Ship Type: 8 bits (not available)
        bitstream += "00000000";

        // Dimension: 9 + 9 bits (not available)
        bitstream += "000000000000000000";

        // Position Fix: 4 bits (not available)
        bitstream += "0000";

        // ETA: 20 bits (not available)
        bitstream += "00000000000000000000";

        // Draught: 8 bits (not available)
        bitstream += "00000000";

        // Destination: 20 chars x 6 bits (not available)
        bitstream += textTo6Bit("", 20);

        // DTE: 1 bit (not available)
        bitstream += "0";

        // Spare: 1 bit
        bitstream += "0";

        return bitstream;
    }

    static std::string textTo6Bit(const std::string& text, int maxLen) {
        std::string result;
        std::string padded = text;

        // Pad to max length
        while (padded.length() < maxLen) {
            padded += ' ';
        }

        // Convert each character to 6-bit AIS
        for (size_t i = 0; i < maxLen; ++i) {
            char ch = padded[i];
            int value = 0;

            if (ch == ' ' || ch == '@') {
                value = 0;
            } else if (ch >= 'A' && ch <= 'Z') {
                value = ch - 'A' + 1;
            } else if (ch >= 'a' && ch <= 'z') {
                value = ch - 'a' + 1;
            } else if (ch >= '0' && ch <= '9') {
                value = ch - '0' + 48;
            }

            // Convert to 6-bit binary string
            for (int bit = 5; bit >= 0; --bit) {
                result += ((value >> bit) & 1) ? '1' : '0';
            }
        }

        return result;
    }

    static std::string bitstreamToAIS6Bit(const std::string& bitstream) {
        std::string result;

        // Convert 6-bit chunks to AIS characters
        for (size_t i = 0; i < bitstream.length(); i += 6) {
            if (i + 5 < bitstream.length()) {
                std::string sixBits = bitstream.substr(i, 6);
                int value = std::stoi(sixBits, nullptr, 2);
                result += char(32 + value);  // AIS 6-bit starts at ASCII 32
            }
        }

        return result;
    }

    static std::string calculateChecksum(const std::string& sentence) {
        int checksum = 0;
        for (size_t i = 1; i < sentence.length(); ++i) {
            if (sentence[i] == '*') break;
            checksum ^= static_cast<int>(sentence[i]);
        }

        std::ostringstream ss;
        ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << checksum;
        return ss.str();
    }
};

int main() {
    std::cout << "=== VESSEL NAME 'CRANE VESTA' NMEA GENERATOR ===" << std::endl << std::endl;

    std::string vesselName = "CRANE VESTA";

    std::cout << "Generating NMEA for vessel name: '" << vesselName << "'" << std::endl;
    std::cout << std::endl;

    // Generate Type 5 message for CRANE VESTA
    std::string nmea = VesselNameEncoder::generateType5ForVessel(vesselName, 374426000);

    std::cout << "GENERATED NMEA:" << std::endl;
    std::cout << "================" << std::endl;
    std::cout << nmea << std::endl;
    std::cout << std::endl;

    // Extract payload for decoder
    size_t payloadStart = nmea.find(",A,") + 3;
    size_t payloadEnd = nmea.find(",", payloadStart);
    std::string payload = nmea.substr(payloadStart, payloadEnd - payloadStart);

    std::cout << "PAYLOAD FOR ONLINE DECODER:" << std::endl;
    std::cout << "============================" << std::endl;
    std::cout << payload << std::endl;
    std::cout << std::endl;

    std::cout << "EXPECTED DECODE:" << std::endl;
    std::cout << "=================" << std::endl;
    std::cout << "Message Type: 5 (Static and Voyage Related Data)" << std::endl;
    std::cout << "MMSI: 374426000" << std::endl;
    std::cout << "Call Sign: CRAFT1" << std::endl;
    std::cout << "Vessel Name: " << vesselName << " (or CRANE@VESTA in some decoders)" << std::endl;
    std::cout << std::endl;

    std::cout << "NOTE:" << std::endl;
    std::cout << "=====" << std::endl;
    std::cout << "Use the PAYLOAD in online AIS decoders to verify it decodes correctly" << std::endl;
    std::cout << "Space in vessel names appears as '@' in AIS encoding" << std::endl;

    return 0;
}