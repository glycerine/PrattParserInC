#include "stdio.h"
#include <vector>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>

#include "l3path.h"
#include "rmkdir.h"
#include "unistd.h"
#include "dv.h"
#include "quicktype.h"
#include "lex_twopointer.h"

#include "l3ts.pb.h"
#include "l3path.h"
#include "l3ts_common.h"
#include "rmkdir.h"
#include "l3obj.h"
#include "autotag.h"
#include "terp.h"
#include "objects.h"
#include "l3string.h"

#include "l3pratt.h"

#include <list>


using std::vector;

#ifndef _MACOSX
int isnumber(int c) {
    if (c >= 0x30 && c <= 0x39) return 1;
    return 0;
}
#endif

extern quicktype_sys*  qtypesys;

// lex and load: turn a utf-8 text file into a 
// an array of pointers to tokens; two pointers per token,
// indicating where to start and one past the end of the token.



// N.B. tabs: the horrible. they are turned into spaces.


//int enDV = 0;

// in terp.h:
// typedef enum lexstate { code, dquote, slashcode, slashdquote, comment, atom, squote, slashsquote, triple_quote, slash_triple_quote,  } lexst_t;

/* now declared in lex_twopointer.h



void gen_protobuf(
                              char* filename,
                              char* start,
                              char* protostart,
                              long  sz,
                              const vector<char*>& tk, 
                              const vector<char*>& tkend, 
                              const vector<t_typ>& tky,
                              const vector<ptr2new_qtree>& tksem,
                              vector<long>& extendsto,
                              vector<bool>& ignore
                  );

void parsel3(
                              char* filename,
                              char* start,
                              char* protostart,
                              long  sz,
                              const vector<char*>& tk, 
                              const vector<char*>& tkend, 
                              const vector<t_typ>& tky, 
                              const vector<ptr2new_qtree>& tksem,
                              vector<long>& extendsto,
                              vector<bool>& ignore,
                              l3obj** retval,
                              Tag*    retown
                  );

void compute_extends_to(
                              const char* filename,
                              const char* start,
                              const char* protostart,
                              long  sz,
                              const vector<char*>& tk, 
                              const vector<char*>& tkend, 
                              const vector<t_typ>& tky,
                              const vector<ptr2new_qtree>& tksem,
                              vector<long>& extendsto,
                              vector<bool>& ignore
                  );
*/

// convert back into visible
//
// if stop is given then we print from start to stop into *d and following.
// otherwise we just print the first token until stop.
//
// return the number of bytes written to d, not including the last \0.
//
long addlexed(const char* start, const char* stop, char** d /*destination*/, bool chomp) {

    assert(d);
    char* dstart = *d;
    assert(dstart);

    long sz = (stop-start);
    long added = sz;
    
    if (sz == 0) return 0;
    assert(sz > 0);
    char* w = dstart + sz;

    memcpy(dstart, start, sz);
    *w = 0;

    if (chomp) {

        // now chomp back whitespace... so every client doesn't have to.
        char* neww = w-1;
        while(neww > dstart && ( (*neww) =='\n' || (*neww) =='\t' || (*neww) ==' ')) {
            (*neww) ='\0';
            --neww;
            --added;
        }
    }

    return added;
}

void lex(char* start, long sz, vector<char*>& tk, vector<char*>& tkend, vector<t_typ>& tky, vector<ptr2new_qtree>& tksem, const char* filename) {
    assert(start);

    char* p = start;
    char* stop = 0;

    if (sz > 0) {
        stop = start+sz;

    } else if (sz < 0) {
        stop = start - sz; // negative sizes mean permatext owned, just flip it.

    } else {
        assert(0); // zero size to lex???
        return;
    }

    char c = '\0';
    lexst_t state = code;
    quicktype* qt = 0;

    long Ntk = 0;
    long Ntkend = 0;
    long Ntky = 0;
    long Ntksem = 0;

    while (p != stop) {

        c = *p;

#ifdef _DEBUG
           // balance checks
           Ntk = tk.size();
           Ntkend = tkend.size();
           Ntky = tky.size();
           Ntksem = tksem.size();
           assert(Ntk == Ntkend);
           assert(Ntk == Ntky);
           assert(Ntk == Ntksem);
#endif
        
           DV(dumpta(tk, tkend, tky));

        switch(state) {

        case atom:
            if (isalnum(c) || c=='_' || c=='.' || c=='`') {
                // stay in atom. 
                // At first we tried to allow '-' in atom to support scientific notation atoms: 1e-5 as a single number;
                // but no, this is a problem: for i--; as in decrement.
            } else {
                state = code;
                // readjust our initial guess, now that we terminated atom.
                tkend.back() = p;

                // do we recognize the token/operation we just saw?
                // temporarliy zero terminate the atom so
                // we can query for it...
                char putback = *p;
                *p = 0;
                qt = 0;
                t_typ atom_recognized = qtypesys->which_type(tk.back(),&qt);
                *p = putback;
                if (atom_recognized) {
                    tky.back() = atom_recognized;
                    tksem.back() = qt->_qtreenew;
                }

                goto START_NEW_CODE_TOKEN;
            }
            break;

        case code:

        START_NEW_CODE_TOKEN:

                switch(c) {

                case  ' ':
                case '\t':
                    break;

#define  TYSEMPUSH(t_quicktype) \
                    tky.push_back(t_quicktype); \
                    tksem.push_back(new_##t_quicktype); \

#define TCASE(character, t_quicktype) \
                case character: \
                    tk.push_back(p); \
                    tkend.push_back(p+1); \
                    TYSEMPUSH(t_quicktype);     \

#define TCASEB(character, t_quicktype) \
                TCASE(character, t_quicktype); \
                break; \

#define TCASEBSTATE(character, t_quicktype, newstate)    \
                TCASE(character, t_quicktype); \
                state = newstate; \
                break; \

                TCASEB( ',', t_comma);   
                TCASEB('\n', t_newline);   
                TCASEB( ';',  t_semicolon);   
                //                TCASEB( '=',  t_asi);   
   
                TCASEB(')',  t_cpn);   
                TCASEB('{',  t_obr);   
                TCASEB('}',  t_cbr);   
                TCASEB('[',  t_osq);   
                TCASEB(']',  t_csq);   
   
                TCASEB('#',  m_hsh);   
                TCASEB('$',  m_dlr);   
                TCASEB('%',  m_pct);   
                TCASEB(':',  m_cln);   
                TCASEB('?',  m_que);
                TCASEB('~',  m_tld);
   
                TCASEB('\'', t_sqo);

                //TCASEBSTATE('\"', t_ut8, dquote)
                case '\"': 
                    if (p+2 < stop && *(p+1)=='\"' && *(p+2)=='\"') {
                        tk.push_back(p);
                        tkend.push_back(p+3);
                        TYSEMPUSH(t_q3q);
                        //TYSEMPUSH(t_ut8);
                        //                        state = triple_quote;
                        p+=2;
                        state = code;

                    } else {
                        tk.push_back(p);
                        tkend.push_back(p+1);
                        TYSEMPUSH(t_ut8);
                        state = dquote;

                    }
                    break;

                    // math types   

#define TWOCHARDECIDE(firstchar,secondoptionalchar, singlechartype, twochartype) \
                case firstchar: \
                    tk.push_back(p);   \
                    if (*(p+1) == secondoptionalchar) {   \
                        tkend.push_back(p+2);   \
                        TYSEMPUSH(twochartype)   \
                        ++p;   \
                    } else {   \
                        tkend.push_back(p+1);   \
                        TYSEMPUSH(singlechartype)   \
                    }   \
                    break;   \

                    TWOCHARDECIDE('=', '=', t_asi, m_eqe);
                    TWOCHARDECIDE('*', '=', m_mlt, m_mte);
                    TWOCHARDECIDE('<', '=', m_lth, m_lte);
                    TWOCHARDECIDE('>', '=', m_gth, m_gte);
                    TWOCHARDECIDE('^', '=', m_crt, m_cre);   

                    TWOCHARDECIDE('&', '&', t_and, m_lan);
                    TWOCHARDECIDE('|', '|', m_bar, m_lor);   

                    //TCASEB('(',  t_opn);   
                case '(':  //  '(' vs "($" vs "(:"
                    tk.push_back(p);   
                    if (*(p+1) == '$') {   
                        tkend.push_back(p+2);   
                        TYSEMPUSH(t_lsp);   
                        ++p;
                    } else if (*(p+1) == ':') {   
                        tkend.push_back(p+2);   
                        TYSEMPUSH(t_lsc);    // lisp-colon
                        ++p;   
                    } else {
                        tkend.push_back(p+1);   
                        TYSEMPUSH(t_opn);   
                    }   
                    break;
      
                case '/':  //  '/' vs "/=" vs "//"   
                    tk.push_back(p);   
                    if (*(p+1) == '/') {   
                        tkend.push_back(p+2);   
                        TYSEMPUSH(t_cppcomment);   
                        ++p;   
                    } else if (*(p+1) == '=') {   
                        tkend.push_back(p+2);   
                        TYSEMPUSH(m_dve);   
                        ++p;   
                    } else {   
                        tkend.push_back(p+1);   
                        TYSEMPUSH(m_dvn);   
                    }   
                    break;
                    
                case '+':  // + vs += vs ++
                    tk.push_back(p);   
                    if (*(p+1) == '=') {   
                        tkend.push_back(p+2);   
                        TYSEMPUSH(m_peq);   
                        ++p;   
                    } else if (*(p+1) == '+') {   
                        tkend.push_back(p+2);   
                        TYSEMPUSH(m_ppl);   
                        ++p;   
                    } else {   
                        tkend.push_back(p+1);   
                        TYSEMPUSH(m_pls);   
                    }   
                    break;

   
                case '-':  // - vs -= vs -3 (digit)  vs --
                    tk.push_back(p);   
                    if (*(p+1) == '=') {   
                        tkend.push_back(p+2);   
                        TYSEMPUSH(m_meq);   
                        ++p;
                    } else if (*(p+1) == '-') {   
                        tkend.push_back(p+2);   
                        TYSEMPUSH(m_mmn);   
                        ++p;   

#if 0     // can't do this, or b=b-1 won't parse right, and the parser can handle it.

                    } else if (isnumber(*(p+1)) || (*(p+1)=='.')) {   
                        // leave negative numbers together with their number, e.g. -3
                        tkend.push_back(p+2); // guess, fix it later.
                        TYSEMPUSH(t_dou);   
                        state=atom;   
#endif
   
                    } else {   
                        tkend.push_back(p+1);   
                        TYSEMPUSH(m_mns);   
                    }   
                    break;   
   
   
                case '!':  // ! vs != vs !< vs !>   
                    tk.push_back(p);   
                    if (*(p+1) == '=') {   
                        tkend.push_back(p+2);   
                        TYSEMPUSH(m_neq);   
                        ++p;   
                    } else if (*(p+1) == '<') {   
                        tkend.push_back(p+2);   
                        TYSEMPUSH(t_oso);   
                        ++p;   
                    } else if (*(p+1) == '>') {   
                        tkend.push_back(p+2);   
                        TYSEMPUSH(t_ico);   
                        ++p;   
                    } else {   
                        tkend.push_back(p+1);   
                        TYSEMPUSH(m_bng);   
                    }   
                    break;   
   
   
                case '@':    // @ vs @> vs @<   
                    tk.push_back(p);   
                    if (*(p+1) == '>') {   
                        tkend.push_back(p+2);   
                        TYSEMPUSH(t_iso);   
                        ++p;   
                    } else if (*(p+1) == '<') {   
                        tkend.push_back(p+2);   
                        TYSEMPUSH(t_oco);   
                        ++p;   
                    } else {   
                        tkend.push_back(p+1);   
                        TYSEMPUSH(m_ats);   
                    }   
                    break;   

                case '\\': 
                    state = slashcode; 
                    break;

#if 0 // new stuff, wait till we have testing back online
                case '.':
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    {
                        tk.push_back(p); // start of new atom (number)
                        tkend.push_back(p+1); // guess, fix it later.
                        TYSEMPUSH(t_dou); 
                        state=atom;
                    }
                break;
#endif                    

                default:
                    if (isalnum(c) || c=='_' || c=='.' || c=='`') {
                        state = atom;
                        tk.push_back(p); // start of new atom.
                        tkend.push_back(p+1); // guess, to be adjusted at actual end of atom, but keeps our arrays always in balanced (same size).
                        TYSEMPUSH(t_ato);
                    } else {
                        printf("warning: unable to terinate atom / detect code token correctly "
                               "due to unrecognized delimiter '%c'.\n",c);
                        assert(0);
                        exit(1);
                    }
                    break;
                }
            break;

        case dquote:

            switch(c) {
            case '\\': state = slashdquote; break;
            case '\"':
                state = code;
                tkend.back() = p+1;
                break;
            }
            break;

        case triple_quote:
            switch(c) {
            case '\\': state = slash_triple_quote; break;
            case '\"':
                if (p+2 < stop && *(p+1)=='\"' && *(p+2)=='\"') {
                    state = code;
                    tkend.back() = p+3;
                    p +=2;
                }
                break;
            }
            break;

#if 0 // poorly thought out, causes problems for single quotes withing triple quotes. Replace with just a t_sqo token instead.
        case squote:

            switch(c) {
            case '\\': state = slashsquote; break;
            case '\'':
                state = code;
                tkend.back() = p+1;
                break;
            }
            break;
#endif

        case slashcode:
            switch(c) {
            case 'n':
                tk.push_back(p-1);
                tkend.push_back(p+1);
                tky.push_back(t_newline);
                tksem.push_back(new_t_newline);
                break;
                // write a test case first...
                //            case '\"':
                //                tk.push_back(p-1);
                //                tkend.push_back(p+1);
                //                tky.push_back(t_newline);
                //                tksem.push_back(new_t_newline);
                //                break;
            default:
                break;
            }
            state = code;
            break;

        case slashdquote:
            state = dquote;
            break;

        case slashsquote:
            state = squote;
            break;

        case slash_triple_quote:
            state = triple_quote;
            break;
            
        default:
            printf("abort: unrecognized/unused state %d in lex_twopointer.cpp.\n",state);
            assert(0);
            exit(1);
            break;

        } // end switch(state)

        ++p;
    }  // while p != stop

    // did we just finish an atom? we probably didn't terminate it right, so fix that:
    if (state==atom) {
        assert(tky.back() == t_ato || tky.back() == t_dou);
        tkend.back() = p;

    } else if (state == dquote) {
        long bad = tk.size()-1;
        printf("error in lexing: end of input reached with unterminated double quote.  Use \"\"\"triple quotes\"\"\" incorporate multiple lines.\n");
        tk_print_error_context(bad, filename, tk, tkend, tky);
        l3throw(XABORT_TO_TOPLEVEL);

    }

    // put in termination token so that our pratt/tdop parsing can advance enough to process
    //  the next-to-the last token. This is what t_eof is for.

    tk.push_back(stop);
    tkend.push_back(stop);
    tky.push_back(t_eof);
    tksem.push_back(new_t_eof);
    
}

void dump(vector<char*>& tk, vector<char*>& tkend, vector<t_typ>& tky) {

    long N = tk.size();
    char* cur = 0;
    char* end = 0;
    l3path token;
    l3path begend;
    l3path b2;

    printf("tk  and  tkend:\n");
    for(long i =0; i < N; ++i) {
        cur = tk[i];
        end = tkend[i];

        token.clear();
        begend.clear();
        
        begend.p += addlexed(cur, end, &(begend.p));

        printf("... tk[%02ld]:  tky:'%s'  begend:'%s'   tkend[%02ld]:'%c'\n",
               i, tky[i], begend(),
               i, *tkend[i]);
    }

    printf("tkend:\n");
    for(long i =0; i < N; ++i) {
        printf("... tkend[%02ld]: addr:%p   string:'%s'\n",i, tkend[i], tkend[i]);
    }


}



void dumpta(
            vector<char*>& tk, 
            vector<char*>& tkend, 
            vector<t_typ>& tky, 
            vector<long>* ext,
            vector<bool>* ignore,
            qtreefactory* qfac
) {

    long N = tk.size();
    long M = tkend.size();
    assert(N == M);
    l3path begend;
    for(long i =0; i < N; ++i) {
        begend.clear();        
        begend.p += addlexed(tk[i], tkend[i], &(begend.p));

        printf("%02ld : '%s' : %s", i, begend(), tky[i]);
        if (ext) {
            printf("     extendsto:%ld", (*ext)[i]);
        }
        if (ignore) {
            printf("     ignore:%d", (*ignore)[i] ? 1 : 0);
        }

        if (qfac) {
            qtree* node = qfac->node_exists(i);
            if (node) {
                qfac->symbol_print(node,"           ");
            } else {
                printf("       no osym");
            }
        }

        printf("\n");
    }
    
}

void dumplisp(
              char* start,
              char* protostart,
              vector<char*>& tk, 
              vector<char*>& tkend, 
              vector<t_typ>& tky, 
              vector<long>& ext,
              vector<bool>& ignore) {

    long N = tk.size();
    long M = tkend.size();
    assert(N == M);
    l3path begend;

    //long df = protostart - start;
    long df = 0;

    for(long i =0; i < N; ++i) {
        //        if (ignore[i]) continue;

        begend.clear();        
        begend.p += addlexed(tk[i] + df, tkend[i] + df, &(begend.p));

        if (ignore[i]) {
            printf("   IGNORE: %02ld : '%s' : %s    extendsto:%ld    ignore:%d\n", i, begend(), tky[i], ext[i], ignore[i] ? 1 : 0);
        } else {
            printf("%02ld : '%s' : %s    extendsto:%ld    ignore:%d\n", i, begend(), tky[i], ext[i], ignore[i] ? 1 : 0);
        }

        
    }
    
}


//
// lex, parse, compile a .c3 l3 class file to generate .proto file,
//   and a qexp based parse tree.
//

// obj = l3path* with path to file to parse.
//
L3METHOD(compile_c3)
{
    l3path& lexme = *(l3path*)obj;

    vector<char*>   tk;
    vector<char*>   tkend;
    vector<t_typ> tky;
    vector<ptr2new_qtree> tksem; // Pratt's semantic code, per token.

    //    l3path lexme("lexme.l3");
    l3path protoout(lexme);
    protoout.pop_dot();
    l3path lispout(protoout);

    protoout.pushf(".proto");
    lispout.pushf(".l3");

    file_ty ty = FILE_DOES_NOT_EXIST;
    long sz = 0;
    bool exists = file_exists(lexme(), &ty, &sz);

    if (!exists || (ty != REGFILE && ty != SYMLINK)) {
        printf("error in compile_c3: got bad file name '%s': not a readable file.\n", lexme());
        exit(1);
    }

    if (sz == 0) {
        printf("error in compile_c3: got empty file '%s'.\n", lexme());
        exit(1);
    }

    // copy for writing .proto
    l3path cmd(0,"cp %s %s",lexme(), protoout());
    if (0 != system(cmd())) {
        printf("error in make: could not create protoout file with '%s'\n",cmd());
        perror("errno indicates");
        exit(1);
    }

    // copy for writing lisp .l3
    cmd.reinit("cp %s %s",lexme(), lispout());
    if (0 != system(cmd())) {
        printf("error in make: could not create lispout file with '%s'\n",cmd());
        perror("errno indicates");
        exit(1);
    }


    long mapsz = sz + 16 ; // at least 16 more so we can point one past the end.

    int fd_proto = open(protoout(), O_RDONLY | O_RDWR);
    if (-1 == fd_proto) {
        l3path msg(0,"error in compile_c3: open of file '%s' failed", protoout());
        perror(msg());
        exit(1);
    }

    int fd_lisp  = open(lispout(), O_RDONLY | O_RDWR);
    if (-1 == fd_lisp) {
        l3path msg(0,"error in compile_c3: open of file '%s' failed", lispout());
        perror(msg());
        exit(1);
    }

    void* start = mmap(0,  // addr
                       mapsz, // len
                       PROT_READ | PROT_WRITE, //prot 
                       MAP_FILE  | MAP_SHARED,  // flags
                       fd_lisp, // fd
                       0   // offset
                       );

    if (start == MAP_FAILED) {
        perror("error in compile_c3: mmap failed for .proto file");
        exit(1);
    }

    void* protostart = mmap(0,  // addr
                       mapsz, // len
                       PROT_READ | PROT_WRITE, //prot 
                       MAP_FILE  | MAP_SHARED,  // flags
                       fd_proto, // fd
                       0   // offset
                       );

    if (protostart == MAP_FAILED) {
        perror("error in compile_c3: mmap failed for .l3 file.");
        exit(1);
    }
    
    DV(
       printf("got starting address from mmap of %p\n", start);
       printf("here is what we see:\n");
       ssize_t written = write(0, start, sz);
       printf("wrote %ld bytes.\n", written);
       );
    
    lex((char*)start, sz, tk, tkend, tky, tksem, lexme());

    //    printf("dump is:\n");
    //    dump(tk, tkend, tky);
    
    DV(
       printf("my tk is:\n");
       dumpta(tk, tkend, tky);    
       )

    vector<long> extendsto(tk.size(),-1); // holds paren matches, and where comments pick up again.
    vector<bool> ignore(tk.size(),false);     // true if in a comment, else false

    gen_protobuf(lexme(), (char*)start, (char*)protostart, sz, tk, tkend, tky, tksem, extendsto, ignore);

    // write protobuf out.
    if (-1 == msync(protostart,mapsz,MS_ASYNC)) {
         perror("error in lex_and_load: msync failed");
         exit(1);         
     }
    munmap(protostart, mapsz);
    close(fd_proto);

    parsel3(lexme(), (char*)start, (char*)0, sz, tk, tkend, tky, tksem, extendsto, ignore,retval,retown);

    // cleanup

     // force write back to disk:
     if (-1 == msync(start,mapsz,MS_ASYNC)) {
         perror("error in lex_and_load: msync failed");
         exit(1);         
     }
    munmap(start, mapsz);
    close(fd_lisp);

    printf("make success: rendered file '%s' from input '%s'.\n",protoout(),lexme());

}
L3END(compile_c3)


qexp::qexp()
    :
    _type(t_qxp)
{ }

qexp::~qexp()
{ }

qexp::qexp(const qexp& src)
{ 
    CopyFrom(src);
}

qexp& qexp::operator=(const qexp& src) {
    if (&src != this) {
        CopyFrom(src);
    }
    return *this;
}

void qexp::CopyFrom(const qexp& src) {


}


#ifdef _LEX_MAIN_
int main(int argc, char** argv) {
    if (argc < 2) {
        printf("error in lex_twopointer.cpp::main(): no file to lex given.\n");
        exit(1);
    }
    l3path lexme(argv[1]);
    compile_c3(lexme);
    return 0;
}
#endif



t_typ which_pbop(const char* p) {
    assert(p);

    t_typ w = qtypesys->which_type(p,0);
    if (!w) return 0;

    if (w == t_pb_message)    return w;
    if (w == t_pb_message)    return w;
    if (w == t_pb_import)     return w;
    if (w == t_pb_package)    return w;
    if (w == t_pb_service)    return w;
    if (w == t_pb_rpc)        return w;
    if (w == t_pb_extensions) return w;
    if (w == t_pb_extend)     return w;
    if (w == t_pb_option)     return w;
    if (w == t_pb_required)   return w;
    if (w == t_pb_repeated)   return w;
    if (w == t_pb_optional)   return w;
    if (w == t_pb_enum)       return w;

    return 0;
}

enum gen_state { NOTPB, NOTPBCOMMENT, IN_PB, IN_PB_COMMENT  };

//
// excluding commented regions between t_cppcomment and t_newline, match up '(' and ')'
//
//
//

inline long mymin(long a, long b) {
    if (a < b) return a;
    return b;
}

//
// use the negative numbers to indicate depth of parenthesis ( or { or [,
//  so that we can tell instantly during parsing if we are inside something
//  that will need closing.
//
void compute_extends_to(const char* filename,
                        const char* start,
                        const char* protostart,
                        long  sz,
                        vector<char*>& tk, 
                        vector<char*>& tkend, 
                        vector<t_typ>& tky,
                        std::vector<ptr2new_qtree>& tksem,
                        vector<long>& extendsto,
                        vector<bool>& ignore,
                        bool  partialokay  // set to true to allow partial parses/unmatched parentheses.
) {

    std::list<long> last_open;

    long toksz = tk.size();

    long i = 0;

#if 0  // now in its own function
    long j = 0;
    long pre_eof = toksz-1;

    // eliminate comments in this first pass:
    for (i=0; i < toksz; ++i ) {
        if (ignore[i]) continue;

        if (tky[i] == t_cppcomment) {
            
            // find the newline, zero everything in between.
            for( j =i ; j < pre_eof && tky[j] != t_newline; ++j) {
                ignore[j]=true;
                assert(tky[j] != t_eof); // sanity check.
            }
            long okay_j = mymin(j,pre_eof);
            extendsto[i] = okay_j; // for comments, extendsto says where to pick up again. we can't go i+1 b/c that might be beyond the end.
            extendsto[okay_j] = i;
            
            if (j < pre_eof)  {
                i = j+1;
            } else break;
        }
    }
#endif

    long negpardepth = 0; // increment indicating depth of parens, in negative number.

    // now match parens. Here extendsto says which parens match each other.
    for (i=0; i < toksz; ++i ) {
        if (extendsto[i] < 0) {
            extendsto[i] += negpardepth;
        }

        if (ignore[i]) continue;
        if (!tk[i]) continue;

        if (tky[i] == t_opn || tky[i] == t_lsp || tky[i] == t_lsc) {
            last_open.push_front(i);
            --negpardepth;
        } 
        else if (tky[i] == t_cpn) {

            // sanity check that we have already seen an open paren:
            if (last_open.size() == 0) {

                if (!partialokay) {
                    long bad = i;
                    printf("error in parsing: unbalanced parenthesis (close with no open paren)\n");
                    tk_print_error_context(bad, filename, tk, tkend, tky);
                    l3throw(XABORT_TO_TOPLEVEL);

                } else {
                    return;
                }

            } else {
                long anch = last_open.front();
                extendsto[anch] = i;
                extendsto[i] =    anch;
                last_open.pop_front();
                ++negpardepth;
            }
        }
    }

    if (!partialokay) {
        
        if (last_open.size()) {
            long bad = last_open.front();
            printf("error in parsing file: unbalanced parenthesis (no matching close paren)\n");
            tk_print_error_context(bad, filename, tk, tkend, tky);
            
            l3throw(XABORT_TO_TOPLEVEL);
            return;
        }
    }

}


void ignore_cpp_comments(const char* filename,
                        const char* start,
                        const char* protostart,
                        long  sz,
                        vector<char*>& tk, 
                        vector<char*>& tkend, 
                        vector<t_typ>& tky,
                        std::vector<ptr2new_qtree>& tksem,
                        vector<long>& extendsto,
                        vector<bool>& ignore,
                        bool  partialokay  // set to true to allow partial parses/unmatched parentheses.
) {

    long toksz = tk.size();

    long i = 0;
    long j = 0;
    long pre_eof = toksz-1;

    // eliminate comments in this first pass:
    for (i=0; i < toksz; ++i ) {
        if (ignore[i]) continue;

        if (tky[i] == t_cppcomment) {
            
            // find the newline, zero everything in between.
            for( j =i ; j < pre_eof && tky[j] != t_newline; ++j) {
                ignore[j]=true;
                assert(tky[j] != t_eof); // sanity check.
            }
            long okay_j = mymin(j,pre_eof);
            extendsto[i] = okay_j; // for comments, extendsto says where to pick up again. we can't go i+1 b/c that might be beyond the end.
            extendsto[okay_j] = i;
            
            if (j < pre_eof)  {
                i = j+1;
            } else break;
        }
    }

}



void keep_only_protobuf(const char*  filename,
                        const char* start,
                        char* protostart,  // blanks written here
                        long  sz,
                        vector<char*>& tk, 
                        vector<char*>& tkend, 
                        vector<t_typ>& tky, 
                        vector<ptr2new_qtree>& tksem,
                        vector<long>& extendsto,
                        vector<bool>& ignore
) {

    long df = protostart - start;

    long toksz = tk.size();
    std::list<long> last_open;
    t_typ pbop = 0;
    const char blank = ' ';

    long i = 0;

    // blank out everything not protobuf
    for (i=0; i < toksz; ++i ) {

        if (ignore[i]) continue;

        if (tky[i] == t_opn || tky[i] == t_lsp || tky[i] == t_lsc) {

            pbop  = which_pbop(tky[i+1]);
            
            if (pbop) {
                last_open.push_front(i);
                *(tk[i] + df) = blank; // blank the open paren
            } else {
                // blank it
                memset(tk[i] + df, blank, tkend[ extendsto[i] ] - tk[i]);
                i = extendsto[i]+1;
                continue;
            }
            
        }
        else if (tky[i] == t_cpn) {

            // sanity check that we have already seen an open paren:
            if (last_open.size() == 0) {
                long bad = i;
                printf("error in parsing: unbalanced parenthesis (close with no open paren)\n");
                tk_print_error_context(bad, filename, tk, tkend, tky);
                l3throw(XABORT_TO_TOPLEVEL);
                return;
            }

            //long anch = last_open.front();
            //extendsto[anch] = i;
            //extendsto[i] =    anch;
            *(tk[i] + df) = blank; // blank the close paren
            last_open.pop_front();
        }
    }
    
    if (last_open.size()) {
        long bad = last_open.front();
        printf("error in parsing: unbalanced parenthesis (no matching close paren)\n");
        tk_print_error_context(bad, filename, tk, tkend, tky);
        l3throw(XABORT_TO_TOPLEVEL);
        return;
    }

}


// gen_protobuf():
// 
// write spaces over the non-protobuf parts, to regenerate a protobuf file.
// simple four state FSM:
//

void gen_protobuf(
                              char* filename,
                              char* start,
                              char* protostart, // write here, leave the start for lisp parsing.
                              long  sz,
                              vector<char*>& tk, 
                              vector<char*>& tkend, 
                              vector<t_typ>& tky, 
                              vector<ptr2new_qtree>& tksem,
                              vector<long>& extendsto,
                              vector<bool>& ignore
                  ) {

    // just write space out the non-protobuf stuff to generate a protobuf file.
    DV(printf("gen_protobuf called.\n"));

    ignore_cpp_comments(filename, start, protostart, sz, tk, tkend, tky, tksem, extendsto, ignore,false);
    compute_extends_to(filename, start, protostart, sz, tk, tkend, tky, tksem, extendsto, ignore,false);

    DV(dumpta(tk, tkend, tky, &extendsto));

    keep_only_protobuf(filename, start, protostart, sz, tk, tkend, tky, tksem, extendsto, ignore);

}


L3METHOD(make)
{
    l3path lexme("tm.c3");
    qqchar tgt;

    if (exp && exp->nchild() > 1) {
        tgt = exp->ith_child(1)->val();
        std::cout << "make sees request for '"<< tgt << "'\n";
        lexme.reinit(tgt);
    } 
    //    else {
    //    printf("");
    //        l3throw(XABORT_TO_TOPLEVEL);
    //    }

    if (tgt.len()) {
        file_ty ty = FILE_DOES_NOT_EXIST;
        long sz = 0;
        bool exists = file_exists(lexme(), &ty, &sz);
        
        if (!exists || (ty != REGFILE && ty != SYMLINK)) {
            printf("error in make: got bad file name '%s': not a readable file.\n", lexme());
            l3throw(XABORT_TO_TOPLEVEL);
        }        
    }

    obj = (l3obj*)&lexme;

    compile_c3(L3STDARGS);
}
L3END(make)



void elim_lisp_style_comments(
                              char* filename,
                              char* start,
                              char* protostart,
                              long  sz,
                              vector<char*>& tk, 
                              vector<char*>& tkend, 
                              vector<t_typ>& tky, 
                              vector<long>& extendsto,
                              vector<bool>& ignore
                              ) {

    long toksz = tk.size();

    long i = 0;
    long j = 0;

    // eliminate lisp comments
    for (i=0; i < toksz; ++i ) {
        if (ignore[i]) continue;

        if (tky[i] == t_semicolon) {
            
            // find the newline, zero everything in between.
            for( j =i ; j < toksz && tky[j] != t_newline; ++j) {
                ignore[j]=true;
            }
            extendsto[i] = j+1; // for comments, extendsto says where to pick up again.
            
            if (j < toksz)  {
                i = j+1;
            } else break;
        }
    }
}



inline bool is_cppcom(const char* p) {
    if (*p != '/') return false;
    if (*(p+1) != '/') return false;
    return true;
}

//
// already done in  compute_extends_to().
//
void elim_cpp_style_comments(
                              char* filename,
                              char* start,
                              char* protostart,
                              long  sz,
                              const vector<char*>& tk, 
                              const vector<char*>& tkend, 
                              const vector<t_typ>& tky, 
                              vector<long>& extendsto,
                              vector<bool>& ignore
                              ) {

    long toksz = tk.size();

    long i = 0;
    long j = 0;

    // eliminate cpp comments
    for (i=0; i < toksz; ++i ) {
        if (ignore[i]) continue;

        if (tky[i] == t_cppcomment) {
            
            // find the newline, zero everything in between.
            for( j =i ; j < toksz && tky[j] != t_newline; ++j) {
                ignore[j]=true;
            }
            extendsto[i] = j+1; // for comments, extendsto says where to pick up again.
            
            if (j < toksz)  {
                i = j+1;
            } else break;
        }
    }
}



void match_braces(
                  const char* filename,
                  char* start,
                  char* protostart,
                  long  sz,
                  vector<char*>& tk, 
                  vector<char*>& tkend, 
                  vector<t_typ>& tky, 
                  vector<ptr2new_qtree>& tksem,
                  vector<long>& extendsto,
                  vector<bool>& ignore,
                  bool partialokay
                  ){

    std::list<long> last_open;

    long toksz = tk.size();
    long i = 0;

    long negpardepth = 0; // increment indicating depth of braces, in negative number.


    // now match braces, saving in extendsto.
    for (i=0; i < toksz; ++i ) {
        if (extendsto[i] < 0) {
            extendsto[i] += negpardepth;
        }

        if (ignore[i]) continue;
        if (!tk[i]) continue;

        if (tky[i] == t_obr) {
            last_open.push_front(i);
            --negpardepth;
        } 
        else if (tky[i] == t_cbr) {

            // sanity check that we have already seen an open paren:
            if (last_open.size() == 0) {

                if (!partialokay) {
                    long bad = i;
                    printf("error in parsing: unbalanced braces (close with no open brace)\n");
                    tk_print_error_context(bad, filename, tk, tkend, tky);
                    l3throw(XABORT_TO_TOPLEVEL);
                    return;
                }

                // partial is okay, but we have seen no open before this close... hmm...
                printf("warning in parsing braces {}: saw close brace '}' before matching open brace '{'.\n");
                continue;
            }

            long anch = last_open.front();
            extendsto[anch] = i;
            extendsto[i] =    anch;
            last_open.pop_front();
            ++negpardepth;
        }
    }

    if (!partialokay) {
        if (last_open.size()) {
            long bad = last_open.front();
            printf("error in parsing: unbalanced braces (no matching close brace)\n");
            tk_print_error_context(bad, filename, tk, tkend, tky);
            l3throw(XABORT_TO_TOPLEVEL);
            return;
        }
    }
}


void match_square_brackets(
                  const char* filename,
                  char* start,
                  char* protostart,
                  long  sz,
                  vector<char*>& tk, 
                  vector<char*>& tkend, 
                  vector<t_typ>& tky, 
                  vector<ptr2new_qtree>& tksem,
                  vector<long>& extendsto,
                  vector<bool>& ignore,
                  bool partialokay
                  ){

    std::list<long> last_open;

    long toksz = tk.size();

    long i = 0;

    long negpardepth = 0; // increment indicating depth of brackets, in negative number.

    // now match square brackets, saving in extendsto.
    for (i=0; i < toksz; ++i ) {
        if (extendsto[i] < 0) {
            extendsto[i] += negpardepth;
        }

        if (ignore[i]) continue;
        if (!tk[i]) continue;

        if (tky[i] == t_osq) {
            last_open.push_front(i);
            --negpardepth;
        } 
        else if (tky[i] == t_csq) {

            // sanity check that we have already seen an open paren:
            if (!partialokay) {
                if (last_open.size() == 0) {
                    long bad = i;
                    printf("error in parsing: unbalanced square brackets (close ']' with no open bracket '[')\n");
                    tk_print_error_context(bad, filename, tk, tkend, tky);
                    l3throw(XABORT_TO_TOPLEVEL);
                    return;
                }
            }

            long anch = last_open.front();
            extendsto[anch] = i;
            extendsto[i] =    anch;
            last_open.pop_front();
            ++negpardepth;
        }
    }
    
    if (!partialokay) {
        if (last_open.size()) {
            long bad = last_open.front();
            printf("error in parsing: unbalanced square brackets (no matching close bracket ']')\n");
            tk_print_error_context(bad, filename, tk, tkend, tky);
            l3throw(XABORT_TO_TOPLEVEL);
            return;
        }
    }
}



//
// match """ and set ignore so that other matchers ignore the text inside the triple quotes
//
void match_triple_quotes(
                  const char* filename,
                  char* start,
                  char* protostart,
                  long  sz,
                  vector<char*>& tk, 
                  vector<char*>& tkend, 
                  vector<t_typ>& tky, 
                  vector<ptr2new_qtree>& tksem,
                  vector<long>& extendsto,
                  vector<bool>& ignore,
                  bool partialokay
                  ){

    long toksz = tk.size();

    long i = 0;

    // three redundant state variables:
    long last_3quote = -1;
    bool in_3quote = false;
    long negpardepth = 0; // increment indicating depth of brackets, in negative number.
    t_typ ty;

    // now match triple quotes, saving in extendsto so we ask for more when inside.
    for (i=0; i < toksz; ++i ) {
        if (extendsto[i] < 0) {
            extendsto[i] += negpardepth;
        }

        if (ignore[i]) continue;
        if (!tk[i]) continue;

        // flip flop in/out of triple quotes, upon seeing """
        ty = tky[i];
        if (ty == t_q3q) {
            if (false == in_3quote) {
                in_3quote = true;
                negpardepth = -1;
                last_3quote = i;

            } else {
                // we were in a quote

                if (last_3quote >=0) {
                    extendsto[i]=last_3quote;
                    extendsto[last_3quote]=i;
                    last_3quote = -1;
                }
                in_3quote = false;
                negpardepth = 0;
                last_3quote = i;
            }
        } else {
            // not on a triple-quote itself, but if we are in the middle of 
            //  one, tell the other balancers to ignore these tokens!
            if (in_3quote && ty != t_eof) {
                ignore[i]=true;
            }
        }

    } // end for.
    
}

void mark_within_single_quotes_as_ignored(
                  const char* filename,
                  char* start,
                  char* protostart,
                  long  sz,
                  vector<char*>& tk, 
                  vector<char*>& tkend, 
                  vector<t_typ>& tky, 
                  vector<ptr2new_qtree>& tksem,
                  vector<long>& extendsto,
                  vector<bool>& ignore,
                  bool partialokay
                  ){

    long toksz = tk.size();

    long i = 0;

    // three redundant state variables:
    long last_single_quote = -1;
    bool in_single_quote = false;
    long negpardepth = 0; // increment indicating depth of brackets, in negative number.
    t_typ ty;

    // now match triple quotes, saving in extendsto so we ask for more when inside.
    for (i=0; i < toksz; ++i ) {
        if (extendsto[i] < 0) {
            extendsto[i] += negpardepth;
        }

        if (ignore[i]) continue;
        if (!tk[i]) continue;

        // flip flop in/out of single quotes, upon seeing '
        ty = tky[i];
        if (ty == t_sqo) {
            if (false == in_single_quote) {
                in_single_quote = true;
                negpardepth = -1;
                last_single_quote = i;

            } else {
                // we were in a single quote

                if (last_single_quote >=0) {
                    extendsto[i]=last_single_quote;
                    extendsto[last_single_quote]=i;
                    last_single_quote = -1;
                }
                in_single_quote = false;
                negpardepth = 0;
                last_single_quote = i;
            }
        } else {
            // not on a triple-quote itself, but if we are in the middle of 
            //  one, tell the other balancers to ignore these tokens!
            if (in_single_quote && ty != t_eof) {
                ignore[i]=true;
            }
        }

    } // end for.
    
}




void keep_only_l3(const char* filename,
                  char* start,   // written here
                  const char* protostart,
                  long  sz,
                  vector<char*>& tk, 
                  vector<char*>& tkend, 
                  vector<t_typ>& tky, 
                  vector<ptr2new_qtree>& tksem,
                  vector<long>& extendsto,
                  vector<bool>& ignore
) {

    long toksz = tk.size();
    std::list<long> last_open;
    t_typ pbop = 0;
    const char blank = ' ';

    //    long df = protostart - start; // add df to translate 
    long df = 0;

    long i = 0;

    // blank out everything that is *not* l3

    for (i=0; i < toksz; ++i ) {
        if (tky[i] == t_cppcomment) {
            // blank the comments, but not the newline that extendsto points to. (so use tk[extendsto[i]], not tkend[extendsto[i]]).
            memset(tk[i] + df,blank,tk[ extendsto[i] ] - tk[i]);
        }
        else if (tky[i] == t_newline) {
            // keep newlines
        }
        else if (ignore[i]) {
            // blank requested, so do it:
            memset(tk[i] + df, blank, tkend[i] - tk[i]);
        } 
        else if (tky[i] == t_opn || tky[i] == t_lsp || tky[i] == t_lsc) {

            pbop  = which_pbop(tky[i+1]);
            if (pbop) {

                // blank the protobuf operations in parens
                memset(tk[i] + df,blank,tkend[ extendsto[i] ] - tk[i]);
            }
            i = extendsto[i];
        }
        else if (tky[i] == t_cpn) {

        }
        else {
            // not in parens, so is not lisp...blank it out
            memset(tk[i] + df, blank, tkend[i] - tk[i]);
        }
    }

}





void parsel3(
                              char* filename,
                              char* start,
                              char* protostart,
                              long  sz,
                              vector<char*>& tk, 
                              vector<char*>& tkend, 
                              vector<t_typ>& tky,
                              vector<ptr2new_qtree>& tksem,
                              vector<long>& extendsto,
                              vector<bool>& ignore,
                              l3obj** retval,
                              Tag*    retown
             ) {

    // just write space out the non-protobuf stuff to generate a protobuf file.
    DV(printf("parsel3 called.\n"));

    DV(dumplisp(start, protostart, tk, tkend, tky, extendsto, ignore));

    match_braces(filename, start, protostart, sz, tk, tkend, tky, tksem, extendsto, ignore, false);
    DV(printf("after matching braces:\n"));
    DV(dumplisp(start, protostart, tk, tkend, tky, extendsto, ignore));

    match_square_brackets(filename, start, protostart, sz, tk, tkend, tky, tksem, extendsto, ignore, false);
    DV(printf("after matching square brackets:\n"));
    DV(dumplisp(start, protostart, tk, tkend, tky, extendsto, ignore));



    pratt(filename, start, protostart, sz, tk, tkend, tky, tksem, extendsto, ignore, retval, retown,0);

    keep_only_l3(filename, start, protostart, sz, tk, tkend, tky, tksem, extendsto, ignore);

}

