#include "nengine/assets/scriptable_data.hpp"
#include "nengine/assets/bounded_input.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>
#include <utility>

namespace nengine::assets {
namespace {
bool fail(std::string* error,const char* why){
    if(error)*error=why;return false;
}
bool valid_key(const std::string& key){
    if(key.empty()||key.size()>96)return false;
    for(char c:key)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
                         (c>='0'&&c<='9')||c=='_'||c=='.'||c=='-'))
        return false;
    return true;
}
}
bool ScriptableData::valid() const noexcept {
    if(values.size()>1024)return false;
    for(const auto& [key,value]:values){
        if(!valid_key(key))return false;
        if(const auto* n=std::get_if<double>(&value);n&&!std::isfinite(*n))
            return false;
        if(const auto* s=std::get_if<std::string>(&value);s&&s->size()>16384)
            return false;
    }
    return true;
}
bool write_scriptable_data(std::ostream& out,const ScriptableData& data,std::string* error){
    if(!data.valid())return fail(error,"invalid scriptable entries");
    std::vector<std::string> ordered;
    for(const auto& [name,_]:data.values)ordered.push_back(name);
    std::sort(ordered.begin(),ordered.end());
    std::ostringstream staged;
    staged<<std::setprecision(std::numeric_limits<double>::max_digits10);
    staged<<"NENGINE_SCRIPTABLE 1\nENTRIES "<<ordered.size()<<"\n";
    for(const auto& key:ordered){
        const auto& val=data.values.at(key);
        staged<<"ENTRY "<<std::quoted(key)<<' ';
        if(auto v=std::get_if<bool>(&val)) staged<<"BOOL "<<(*v?1:0);
        else if(auto v=std::get_if<std::int64_t>(&val))staged<<"INT "<<*v;
        else if(auto v=std::get_if<double>(&val))staged<<"FLOAT "<<*v;
        else staged<<"STRING "<<std::quoted(std::get<std::string>(val));
        staged<<'\n';
    }
    staged<<"END_SCRIPTABLE\n";
    const auto text=staged.str();
    if(text.size()>1024u*1024u)return fail(error,"scriptable data too large");
    out<<text;
    if(!out.good())return fail(error,"scriptable output stream failed");
    if(error)error->clear();return true;
}
bool read_scriptable_data(std::istream& in,ScriptableData& result,std::string* error){
    std::string bytes;
    if(!read_bounded_text(in,bytes,1024u*1024u))
        return fail(error,"scriptable source exceeds 1 MiB or read failed");
    std::istringstream source{bytes};
    ScriptableData staged;
    std::string tag;unsigned version=0;std::size_t n=0;
    if(!(source>>tag>>version)||tag!="NENGINE_SCRIPTABLE"||version!=1||
       !(source>>tag>>n)||tag!="ENTRIES"||n>1024)
        return fail(error,"invalid scriptable header");
    for(std::size_t i=0;i<n;++i){
        std::string key,type;
        if(!(source>>tag)||tag!="ENTRY"||
           !(source>>std::quoted(key)>>type)||!valid_key(key)||
           staged.values.contains(key))
            return fail(error,"bad or duplicate scriptable key");
        ScriptableValue val;
        if(type=="BOOL"){
            int b=-1;if(!(source>>b)||(b!=0&&b!=1))
                return fail(error,"bad scriptable bool");
            val=bool(b);
        }else if(type=="INT"){
            std::int64_t num;if(!(source>>num))return fail(error,"bad scriptable int");
            val=num;
        }else if(type=="FLOAT"){
            double num;if(!(source>>num)||!std::isfinite(num))
                return fail(error,"bad scriptable float");
            val=num;
        }else if(type=="STRING"){
            std::string str;if(!(source>>std::quoted(str))||str.size()>16384)
                return fail(error,"bad scriptable string");
            val=std::move(str);
        }else return fail(error,"unknown scriptable value type");
        staged.values.emplace(std::move(key),std::move(val));
    }
    if(!(source>>tag)||tag!="END_SCRIPTABLE"||(source>>tag)||
       !staged.valid())return fail(error,"trailing or invalid scriptable content");
    result=std::move(staged);
    if(error)error->clear();return true;
}
} // namespace nengine::assets
