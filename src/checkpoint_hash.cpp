#include "checkpoint_internal.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace checkpoint::internal{

namespace{

/* ---------------------------------------------------------
 SHA-1実装
--------------------------------------------------------- */
class Sha1{
public:
    Sha1()
        : h0_(0x67452301),
          h1_(0xEFCDAB89),
          h2_(0x98BADCFE),
          h3_(0x10325476),
          h4_(0xC3D2E1F0),
          total_bytes_(0),
          buffer_size_(0) {
        buffer_.fill(0);
    }

    void Update(const char* data, std::size_t size){
        total_bytes_ += static_cast<std::uint64_t>(size);

        while(size > 0){
            const auto copy_size = std::min(size, buffer_.size() - buffer_size_);
            std::copy_n(data, copy_size, buffer_.begin() + static_cast<std::ptrdiff_t>(buffer_size_));
            data += copy_size;
            size -= copy_size;
            buffer_size_ += copy_size;

            if(buffer_size_ == buffer_.size()){
                ProcessBlock(buffer_.data());
                buffer_size_ = 0;
            }
        }
    }

    std::string FinalizeHex(){
        auto block = buffer_;
        auto block_size = buffer_size_;
        const auto total_bits = total_bytes_ * 8;

        block[block_size++] = static_cast<char>(0x80);

        if(block_size > 56){
            std::fill(block.begin() + static_cast<std::ptrdiff_t>(block_size), block.end(), 0);
            ProcessBlock(block.data());
            block.fill(0);
            block_size = 0;
        }

        std::fill(
            block.begin() + static_cast<std::ptrdiff_t>(block_size),
            block.begin() + 56,
            0
        );

        for(int i = 0; i < 8; ++i){
            block[63 - i] = static_cast<char>((total_bits >> (i * 8)) & 0xFF);
        }

        ProcessBlock(block.data());

        std::array<std::uint8_t, 20> digest{};
        WriteWord(digest, 0, h0_);
        WriteWord(digest, 4, h1_);
        WriteWord(digest, 8, h2_);
        WriteWord(digest, 12, h3_);
        WriteWord(digest, 16, h4_);

        static constexpr char kHex[] = "0123456789abcdef";
        std::string hex;
        hex.reserve(digest.size() * 2);

        for(auto value : digest){
            hex.push_back(kHex[(value >> 4) & 0x0F]);
            hex.push_back(kHex[value & 0x0F]);
        }

        return hex;
    }

private:
    static std::uint32_t ReadWord(const char* block){
        return (static_cast<std::uint32_t>(static_cast<unsigned char>(block[0])) << 24) |
               (static_cast<std::uint32_t>(static_cast<unsigned char>(block[1])) << 16) |
               (static_cast<std::uint32_t>(static_cast<unsigned char>(block[2])) << 8) |
               static_cast<std::uint32_t>(static_cast<unsigned char>(block[3]));
    }

    static void WriteWord(std::array<std::uint8_t, 20>& digest, std::size_t offset, std::uint32_t value){
        digest[offset + 0] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
        digest[offset + 1] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
        digest[offset + 2] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
        digest[offset + 3] = static_cast<std::uint8_t>(value & 0xFF);
    }

    void ProcessBlock(const char* block){
        std::array<std::uint32_t, 80> words{};
        for(std::size_t i = 0; i < 16; ++i){
            words[i] = ReadWord(block + (i * 4));
        }

        for(std::size_t i = 16; i < words.size(); ++i){
            words[i] = std::rotl(words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16], 1);
        }

        auto a = h0_;
        auto b = h1_;
        auto c = h2_;
        auto d = h3_;
        auto e = h4_;

        for(std::size_t i = 0; i < words.size(); ++i){
            std::uint32_t f = 0;
            std::uint32_t k = 0;

            if(i < 20){
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            }else if(i < 40){
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            }else if(i < 60){
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            }else{
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            const auto temp = std::rotl(a, 5) + f + e + k + words[i];
            e = d;
            d = c;
            c = std::rotl(b, 30);
            b = a;
            a = temp;
        }

        h0_ += a;
        h1_ += b;
        h2_ += c;
        h3_ += d;
        h4_ += e;
    }

    std::uint32_t h0_;
    std::uint32_t h1_;
    std::uint32_t h2_;
    std::uint32_t h3_;
    std::uint32_t h4_;
    std::uint64_t total_bytes_;
    std::size_t buffer_size_;
    std::array<char, 64> buffer_;
};


/**
 * @brief 入力ストリーム全体のSHA-1ハッシュを計算する
 *
 * @param input ハッシュ計算対象の入力ストリーム
 * @return std::string 16進数表現のSHA-1ハッシュ
 */
std::string ComputeHash(std::istream& input){
    Sha1 sha1;
    std::array<char, kHashChunkSize> buffer{};

    while(input){
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto read_size = input.gcount();
        if(read_size > 0){
            sha1.Update(buffer.data(), static_cast<std::size_t>(read_size));
        }
    }

    if(!input.eof() && input.fail()){
        throw std::runtime_error(BuildRuntimeErrorMessage(
            "Failed to read stream for hashing",
            std::string("stream read ended before EOF")
        ));
    }

    return sha1.FinalizeHex();
}

}


/**
 * @brief ファイル内容のSHA-1ハッシュを計算する
 *
 * @param file_path ハッシュ計算対象のファイルパス
 * @return std::string 16進数表現のSHA-1ハッシュ
 */
std::string ComputeFileHash(const std::filesystem::path& file_path){
    std::ifstream input(file_path, std::ios::binary);
    if(!input){
        throw std::runtime_error(BuildRuntimeErrorMessage(
            "Failed to open file for hashing",
            file_path
        ));
    }

    return ComputeHash(input);
}


/**
 * @brief 文字列内容のSHA-1ハッシュを計算する
 *
 * @param text ハッシュ計算対象の文字列
 * @return std::string 16進数表現のSHA-1ハッシュ
 */
std::string ComputeStringHash(const std::string& text){
    std::istringstream input(text);
    return ComputeHash(input);
}

}
