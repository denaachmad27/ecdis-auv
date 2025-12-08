#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <bitset>

class AISCompleteEncoder {
public:
    static std::vector<std::string> encodeType5Complete(int mmsi, const std::string& callsign, const std::string& name) {
        // Build complete bitstream for AIS Type 5 message
        std::string bitstream = buildType5Bitstream(mmsi, callsign, name);

        // Convert bitstream to 6-bit encoded AIS payload
        std::string payload = bitstreamToAIS6Bit(bitstream);

        // Split into NMEA AIVDM fragments
        return splitToVDMFragments(payload);
    }

private:
    static std::string buildType5Bitstream(int mmsi, const std::string& callsign, const std::string& name) {
        std::string bitstream;

        // Message Type: 5 (6 bits) - Static and Voyage Related Data
        bitstream += std::bitset<6>(5).to_string();

        // Repeat Indicator: 0 (2 bits) - Original message
        bitstream += std::bitset<2>(0).to_string();

        // MMSI: 30 bits
        bitstream += std::bitset<30>(mmsi).to_string();

        // AIS Version: 0 (2 bits) - AIS station compliant
        bitstream += std::bitset<2>(0).to_string();

        // IMO Number: 30 bits - Not available (0)
        bitstream += std::bitset<30>(0).to_string();

        // Call Sign: 7 characters x 6 bits = 42 bits
        bitstream += textTo6Bit(callsign, 7);

        // Vessel Name: 20 characters x 6 bits = 120 bits
        bitstream += textTo6Bit(name, 20);

        // Ship Type: 8 bits - Not available (0)
        bitstream += std::bitset<8>(0).to_string();

        // Dimension/Bow to Stern: 9 bits - Not available (0)
        bitstream += std::bitset<9>(0).to_string();

        // Dimension/Port to Starboard: 9 bits - Not available (0)
        bitstream += std::bitset<9>(0).to_string();

        // Position Fix Type: 4 bits - Not available (0)
        bitstream += std::bitset<4>(0).to_string();

        // ETA: 20 bits - Not available (0)
        bitstream += std::bitset<20>(0).to_string();

        // Draught: 8 bits - Not available (0)
        bitstream += std::bitset<8>(0).to_string();

        // Destination: 20 characters x 6 bits = 120 bits - Not available
        bitstream += textTo6Bit("", 20);

        // DTE: 1 bit - Data terminal equipment not ready (0)
        bitstream += std::bitset<1>(0).to_string();

        // Spare: 1 bit - Not used (0)
        bitstream += std::bitset<1>(0).to_string();

        return bitstream;
    }

    static std::string textTo6Bit(const std::string& text, int maxLen) {
        std::string result;
        std::string paddedText = text;

        // Pad with spaces to max length
        while (paddedText.length() < maxLen) {
            paddedText += ' ';
        }

        // Convert each character to 6-bit AIS representation
        for (size_t i = 0; i < paddedText.length() && i < maxLen; ++i) {
            char ch = paddedText[i];
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

            // Convert value to 6-bit binary
            result += std::bitset<6>(value).to_string();
        }

        return result;
    }

    static std::string bitstreamToAIS6Bit(const std::string& bitstream) {
        std::string payload;

        // Convert 6-bit chunks to AIS 6-bit characters (ASCII 32-95)
        for (size_t i = 0; i < bitstream.length(); i += 6) {
            if (i + 5 < bitstream.length()) {
                std::string sixBits = bitstream.substr(i, 6);
                int value = std::stoi(sixBits, nullptr, 2);
                payload += char(32 + value);  // AIS 6-bit starts at ASCII 32
            }
        }

        return payload;
    }

    static std::vector<std::string> splitToVDMFragments(const std::string& payload) {
        std::vector<std::string> fragments;
        const int maxPayloadPerFragment = 61;  // Maximum for AIS AIVDM messages

        int totalFragments = (payload.length() + maxPayloadPerFragment - 1) / maxPayloadPerFragment;

        for (int fragNum = 0; fragNum < totalFragments; ++fragNum) {
            int startPos = fragNum * maxPayloadPerFragment;
            int fragPayloadLen = std::min(maxPayloadPerFragment, (int)payload.length() - startPos);

            std::string fragPayload = payload.substr(startPos, fragPayloadLen);

            // Build AIVDM sentence
            std::ostringstream sentence;
            sentence << "!AIVDM,"                     // Talker ID
                     << totalFragments << ","          // Total fragments
                     << (fragNum + 1) << ","           // Current fragment
                     << ","                            // Sequence number (not used)
                     << "A,"                           // Channel A
                     << fragPayload << ","             // Payload
                     << "0*";                          // Fill bits

            std::string sentenceWithoutChecksum = sentence.str();
            sentenceWithoutChecksum += calculateChecksum(sentenceWithoutChecksum);

            fragments.push_back(sentenceWithoutChecksum);
        }

        return fragments;
    }

    static std::string calculateChecksum(const std::string& sentence) {
        int checksum = 0;

        // Calculate checksum from first character after '!' to '*'
        for (size_t i = 1; i < sentence.length(); ++i) {
            if (sentence[i] == '*') break;
            checksum ^= static_cast<int>(sentence[i]);
        }

        std::ostringstream hexChecksum;
        hexChecksum << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << checksum;
        return hexChecksum.str();
    }
};

int main() {
    std::cout << "=== COMPLETE AIS TYPE 5 ENCODER FOR CRANE VESTA ===" << std::endl << std::endl;

    // CRANE VESTA parameters
    int mmsi = 374426000;
    std::string callsign = "3EWC6";
    std::string vesselName = "CRANE VESTA";

    std::cout << "VESSEL PARAMETERS:" << std::endl;
    std::cout << "==================" << std::endl;
    std::cout << "MMSI: " << mmsi << std::endl;
    std::cout << "Call Sign: '" << callsign << "'" << std::endl;
    std::cout << "Vessel Name: '" << vesselName << "'" << std::endl;
    std::cout << "Message Type: 5 (Static and Voyage Related Data)" << std::endl;
    std::cout << std::endl;

    // Generate complete AIS Type 5 message
    std::vector<std::string> nmeaFragments = AISCompleteEncoder::encodeType5Complete(mmsi, callsign, vesselName);

    std::cout << "GENERATED AIVDM FRAGMENTS:" << std::endl;
    std::cout << "==========================" << std::endl;
    for (size_t i = 0; i < nmeaFragments.size(); ++i) {
        std::cout << "Fragment " << (i + 1) << ": " << nmeaFragments[i] << std::endl;
    }
    std::cout << std::endl;

    // Extract payloads for online decoder testing
    std::cout << "PAYLOADS FOR ONLINE DECODER:" << std::endl;
    std::cout << "=============================" << std::endl;
    for (size_t i = 0; i < nmeaFragments.size(); ++i) {
        size_t payloadStart = nmeaFragments[i].find(",A,") + 3;
        size_t payloadEnd = nmeaFragments[i].find(",", payloadStart);
        if (payloadStart != std::string::npos && payloadEnd != std::string::npos) {
            std::string payload = nmeaFragments[i].substr(payloadStart, payloadEnd - payloadStart);
            std::cout << "Payload " << (i + 1) << ": " << payload << std::endl;
        }
    }
    std::cout << std::endl;

    // Show expected decode results
    std::cout << "EXPECTED DECODE RESULTS:" << std::endl;
    std::cout << "========================" << std::endl;
    std::cout << "Message Type: 5 (Static and Voyage Related Data)" << std::endl;
    std::cout << "MMSI: " << mmsi << std::endl;
    std::cout << "IMO Number: 0 (Not available)" << std::endl;
    std::cout << "Call Sign: " << callsign << std::endl;
    std::cout << "Vessel Name: " << vesselName << std::endl;
    std::cout << "Ship Type: 0 (Not available)" << std::endl;
    std::cout << "Dimension: 0 x 0 meters" << std::endl;
    std::cout << "Destination: (Not available)" << std::endl;
    std::cout << std::endl;

    std::cout << "NOTE: Space in vessel name appears as '@' in AIS encoding" << std::endl;
    std::cout << "So 'CRANE VESTA' may appear as 'CRANE@VESTA' in decoder output" << std::endl;

    return 0;
}