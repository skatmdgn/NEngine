#include "nengine/player/save_data.hpp"
#include <array>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <random>
#include <regex>
#include <system_error>
#include <utility>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

namespace nengine::player {
namespace {
constexpr std::size_t max_save_bytes=4u*1024u*1024u;
bool bad(std::string* error,const char* message){
    if(error)*error=message;
    return false;
}
bool valid_slot(const std::string& name){
    if(name.empty()||name.size()>64)return false;
    for(char c:name)
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
             (c>='0'&&c<='9')||c=='-'||c=='_'))return false;
    return true;
}
std::uint32_t crc(const std::vector<std::uint8_t>& bytes){
    std::uint32_t hash=0xFFFFFFFFu;
    for(auto b:bytes){
        hash^=b;
        for(int i=0;i<8;++i)hash=(hash>>1)^(hash&1u?0xEDB88320u:0);
    }
    return ~hash;
}
void put_u32(std::ostream& output,std::uint32_t n){
    for(int i=0;i<4;++i)output.put(static_cast<char>((n>>(8*i))&255));
}
bool read_u32(std::istream& input,std::uint32_t& n){
    n=0;
    for(int i=0;i<4;++i){
        auto c=input.get();
        if(c==std::char_traits<char>::eof())return false;
        n|=static_cast<std::uint32_t>(static_cast<unsigned char>(c))<<(8*i);
    }
    return true;
}
bool replace_atomic(const std::filesystem::path& from,
                    const std::filesystem::path& target,
                    std::error_code& ec){
#if defined(_WIN32)
    if(!::MoveFileExW(from.c_str(),target.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)){
        ec=std::error_code(static_cast<int>(::GetLastError()),std::system_category());
        return false;
    }
    ec.clear();
    return true;
#else
    std::filesystem::rename(from,target,ec);
    return !ec;
#endif
}
}
std::optional<std::filesystem::path> GameSaveStore::user_data_directory(
    std::string app_id){
    if(!valid_slot(app_id))return std::nullopt;
#if defined(_WIN32)
    const wchar_t* value=_wgetenv(L"LOCALAPPDATA");
    if(!value || !*value)return std::nullopt;
    return std::filesystem::path(value)/"NEngine"/app_id;
#else
    const char* xdg=std::getenv("XDG_DATA_HOME");
    if(xdg && *xdg && std::filesystem::path(xdg).is_absolute())
        return std::filesystem::path(xdg)/"NEngine"/app_id;
    const char* home=std::getenv("HOME");
    if(!home || !*home)return std::nullopt;
    return std::filesystem::path(home)/".local"/"share"/"NEngine"/app_id;
#endif
}
bool GameSaveStore::write(std::string slot,const GameSave& data,std::string* error) const {
    if(!valid_slot(slot)||data.version==0||data.bytes.size()>max_save_bytes)
        return bad(error,"invalid save slot, version or payload size");
    std::error_code ec;
    if(root_.empty()||root_.is_relative()||std::filesystem::is_symlink(root_,ec)||ec)
        return bad(error,"invalid/symlinked game save directory");
    std::filesystem::create_directories(root_,ec);
    if(ec||std::filesystem::is_symlink(root_,ec)||ec)
        return bad(error,"cannot initialize game save directory");
    const auto target=root_/(slot+".nsave");
    if(std::filesystem::is_symlink(target,ec)||ec)
        return bad(error,"save target cannot be a symlink");
    // Exclusive temp name prevents other writers from truncating our data.
    // Filename entropy is not a security token; it only avoids collisions.
    static std::atomic<std::uint64_t> generation{0};
    const auto temp=root_/("." + slot + ".nsave-" +
                          std::to_string(++generation)+".tmp");
    if(std::filesystem::exists(temp,ec)||ec)
        return bad(error,"game save staging conflict");
    {
        std::ofstream out(temp,std::ios::binary|std::ios::trunc);
        if(!out)return bad(error,"cannot open save staging file");
        out.write("NENGSAVE",8);
        put_u32(out,1u);put_u32(out,data.version);
        put_u32(out,static_cast<std::uint32_t>(data.bytes.size()));
        put_u32(out,crc(data.bytes));
        out.write(reinterpret_cast<const char*>(data.bytes.data()),
                  static_cast<std::streamsize>(data.bytes.size()));
        out.flush();
        if(!out.good()){
            out.close();std::filesystem::remove(temp,ec);
            return bad(error,"cannot write save staging file");
        }
    }
    if(!replace_atomic(temp,target,ec)){
        std::error_code ignored;
        std::filesystem::remove(temp,ignored);
        return bad(error,"cannot atomically commit game save");
    }
    if(error)error->clear();
    return true;
}
bool GameSaveStore::read(std::string slot,GameSave& output,std::string* error) const {
    if(!valid_slot(slot))return bad(error,"invalid save slot");
    std::error_code ec;
    const auto path=root_/(slot+".nsave");
    if(root_.empty()||std::filesystem::is_symlink(root_,ec)||ec||
       std::filesystem::is_symlink(path,ec)||ec||
       !std::filesystem::is_regular_file(path,ec)||ec)
        return bad(error,"missing/unsafe save data");
    const auto size=std::filesystem::file_size(path,ec);
    if(ec||size<24||size>max_save_bytes+24)
        return bad(error,"invalid save file size");
    std::ifstream in(path,std::ios::binary);
    char magic[8]{};
    if(!in.read(magic,8)||std::string(magic,8)!="NENGSAVE")
        return bad(error,"invalid save header");
    std::uint32_t format=0,version=0,count=0,hash=0;
    if(!read_u32(in,format)||!read_u32(in,version)||
       !read_u32(in,count)||!read_u32(in,hash)||format!=1u||version==0||
       count>max_save_bytes||count+24u!=size)
        return bad(error,"invalid save version or payload size");
    GameSave staged;
    staged.version=version;
    staged.bytes.resize(count);
    if(!in.read(reinterpret_cast<char*>(staged.bytes.data()),count)||crc(staged.bytes)!=hash)
        return bad(error,"save checksum mismatch or truncated payload");
    output=std::move(staged);
    if(error)error->clear();
    return true;
}
bool GameSaveStore::read_migrated(std::string slot,std::uint32_t expected_version,
    const SaveMigration& migrate,GameSave& output,std::string* error) const {
    if(expected_version==0)return bad(error,"invalid requested save version");
    GameSave staged;
    if(!read(slot,staged,error))return false;
    if(staged.version>expected_version)
        return bad(error,"save is from a newer application version");
    if(staged.version<expected_version) {
        if(!migrate||!migrate(staged.version,expected_version,staged.bytes)||
           staged.bytes.size()>max_save_bytes)
            return bad(error,"game save migration failed");
        staged.version=expected_version;
    }
    output=std::move(staged);
    if(error)error->clear();
    return true;
}
} // namespace nengine::player
