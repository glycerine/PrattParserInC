#include <stdio.h>
#include <vector>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>
#include <math.h>
#include <climits>

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
#include <list>
#include "l3pratt.h"
#include "l3ts.pb.h"
#include "l3ts_server.h"
#include "quicktype.h"
#include "jlmap.h"

using std::vector;

#ifndef _MACOSX
int isnumber(int c);
#endif

// precedence levels, from Douglas Crockford's article
//
//       (binds less tightly => towards top of parse tree)
//
//     0 non-binding operators like ;
//    10 assignment operators like =
//    20 ?       ...i.e. the ternary operator ? :
//    30 || &&
//    40 relational operators like  eq, >=, !=, <=, <, >
//    50 + -
//    60 * /
//    70 unary operators like ! (for not)
//    80 . [
//
//   100 (
//     (binds more tightly => towards bottom of parse tree)

//
// at first I thought types like 'int' should be prefix operators,
//  but that makes them the top of the tree rather than describing 
//  the atom that is the function/variable name.
//
// so instead try making atoms have small left binding power, and see
//  if they can grab their descriptive adjectives (types).
//

extern quicktype_sys*  qtypesys;


//// printing of qtree in sexpstack dumps of Tag* :

typedef jlmap<qtree*,long> qtlmap;

void incref(qtlmap* map, qtree* t) {
    assert(t);
    long v = 0;

    v=lookupkey(map, t);
    if (v) { 
        insertkey(map,t,v+1); 
    } else {
        assert(0);
    }
}

void printhelp(long& iii, qtree* n, const char* indent, int depth) {

    std::cout << indent << " ->sexp<- "
              << iii 
              << " ser# "<< n->_ser
              << ":  " << n->_ty ;
    for (int i = 0; i < depth; ++i) {
        std::cout << "    \\->";
    }

    std::cout << " '" << n->span() << "'\n";

}

void recurprint(int depth, long& iii, qtlmap* map, qtree* t, const char* indent) {
    assert(t);
    assert(map);

    ++iii;
    printhelp(iii, t, indent, depth);

    if (t->_headnode) { 
        if (lookupkey(map, t->_headnode)) {
            recurprint(depth+1, iii, map, t->_headnode,indent); 
        }
    }

    if (t->_body) { 
        if (lookupkey(map, t->_body)) {
            recurprint(depth+1, iii, map, t->_body,indent); 
        }
    }

    qtree* ch=0;
    long   nc = t->nchild();
    for (long i =0; i < nc; ++i) {
        ch = t->ith_child(i);
        if (ch) { 
            if (lookupkey(map, ch)) {
                recurprint(depth+1, iii, map, ch,indent); 
            }            
        }
    }

}

void print_ustaq_of_sexp_as_outline(ustaq<sexp_t>&  stk, const char* indent) {
    //    long n = stk.size();
    
    // a node is a root if nothing else points to it. count
    // pointed-to here. At the end, roots will have a 1
    // entry in the map. Zero means not found, so we use 1 for roots.

    qtlmap* map = newJudyLmap<qtree*,long>();

    qtree* t = 0;

    // init all to 1
    for(stk.it.restart(); !stk.it.at_end(); ++stk.it) {
        t = *(stk.it);
        insertkey(map, t, 1L);
    }

    qtree* ch = 0;
    long   nc = 0;
    for(stk.it.restart(); !stk.it.at_end(); ++stk.it) {
        t = *(stk.it);
        nc  = t->nchild();
        if (t->_headnode) { incref(map, t->_headnode); }
        if (t->_body) { incref(map, t->_body); }

        for (long i =0; i < nc; ++i) {
            ch = t->ith_child(i);
            if (ch) { incref(map, ch); }
        }
    }

    // now print, starting from roots
    long v = 0;
    long iii = 0;
    int depth = 0;

    for(stk.it.restart(); !stk.it.at_end(); ++stk.it) {
        t = *(stk.it);
        v=lookupkey(map, t);
        if (v==1) { 
            // a root
            recurprint(depth, iii, map, t, indent);
        }

    }

    
    deleteJudyLmap(map);

}



///// end printing of qtree in sexpstack dumps.




void pratt(
                              char* filename,
                              char* to_be_deleted_start,
                              char* to_be_deleted_lispstart,
                              long  sz,
                              vector<char*>& tk,
                              vector<char*>& tkend,
                              vector<t_typ>& tky,
                              vector<ptr2new_qtree>& tksem,
                              vector<long>& extendsto,
                              vector<bool>& ignore,
                              l3obj** retval,
                              Tag*    retown,
                              tdopout_struct* tdo
                 )
{

    qtreefactory* qtfac = new qtreefactory(tk,tkend,tky,tksem,extendsto,ignore,tdo);

    qtfac->advance(0);
    qtree* parsed = qtfac->expression(-1);

    if (parsed) {

    } else {
        printf("pratt: got empty parse tree back.\n");
    }


}


long qtreefactory::request_more_input(parse_cmd& cmd) {
    if (cmd._forbid_calling_get_more_input) return 0;
    
    int feof_fp = 0;
    long nchar_added = get_more_input(cmd._fp, cmd._prompt, cmd._show_prompt_if_not_stdin, cmd._echo_line, true /*show_plus*/, feof_fp);
    if (0==nchar_added) return 0;

    // gotta fix up the lex arrays...
    _tdo->backup_one_elim_eof();
    lex(permatext->frame(), permatext->len(), _tk, _tkend, _tky, _tksem, cmd._filename);

    DV(
       printf("my tk is:\n");
       dumpta(_tk, _tkend, _tky, &_extendsto, &_ignore, this); 
       )

    _tdo->update_extendsto_and_ignore();
    prep_to_resume();

    return nchar_added;
}


void qtreefactory::clear_symtable() {

    // delete the nodes we new-ed up and saved in _symtable.
    oid2qtree_it be = _symtable.begin();
    oid2qtree_it en = _symtable.end();

    qtree* s = 0;
    for (; be != en; ++be) {
        s = be->second;
        if (s) {

            // request delete from Tag, but somebody else
            // may now own it, so don't worry if it's not there.
            if (_tdo && _tdo->_myobj && _tdo->_myobj->_mytag) {
                if (_tdo->_myobj->_mytag->sexpstack_exists(s)) {
                    destroy_sexp(s);
                }
            }
        }
    }
    _symtable.clear();

}

qtreefactory::~qtreefactory() {

    clear_symtable();

}



void qtreefactory::dump_symtable() {

    oid2qtree_it be = _symtable.begin();
    oid2qtree_it en = _symtable.end();

    qtree* s = 0;
    int i = 0;
    for (; be != en; ++be, ++i) {
        s = be->second;
        if (s) {
            printf("_symtable[%02d] =\n",i);
            s->print("   ");
        } else {
            printf("_symtable[%02d] = 0x0",i);
        }
    }
    
}



void qtreefactory::symbol_print(qtree* s, const char* indent) {

    printf("%ssymbol %p is:\n",indent,s);
    if (!s) return;

    l3path begend;
    begend.p += addlexed(s->_val.b,s->_val.e, &(begend.p));

    printf("%s    '%s' : '%s'\n", indent, s->_ty, begend());
    printf("%s     _lbp:%d   _rbp:%d\n", indent, s->_lbp, s->_rbp);
    
    l3path indmore(0,"%s      ",indent);
    l3path indmoremore(0,"%s      ",indmore());

    if (s->_headnode) {
        printf("%s_headnode:\n",indmore());
        symbol_print(s->_headnode, indmoremore());
    }
    
    for (ulong i = 0; i < s->_chld.size(); ++i) {
        printf("%schild %ld :\n",indent,i);

        s->_chld[i]->print(indmore());
    }

    if (s->_chld.size()==0) {
        printf("%s<no children>.\n",indent);
    }
}


qtree*  qtreefactory::node_exists(long id) {

    if (_symtable.size() == 0) return 0;
    oid2qtree_it it = _symtable.find(id);
    if (it != _symtable.end()) {
        return it->second;
    }
    return 0;
}


void qtreefactory::delete_id(long id) {

    oid2qtree_it be = _symtable.begin();
    oid2qtree_it fi = _symtable.find(id);
    oid2qtree_it en = _symtable.end();
    
    DV(
       for (long i = 0; be != en; ++be, ++i) {
           printf("%02ld]: %ld -> %p : %s\n", i, be->first, be->second, be->second->_ty);
       }
       );

    assert(fi != en);
    qtree* s = fi->second;
    _symtable.erase(fi);

    if (s) {
        destroy_sexp(s);
    }
    
}


qtreefactory::qtreefactory(
                     vector<char*>& tk, 
                     vector<char*>& tkend, 
                     vector<t_typ>& tky,
                     vector<ptr2new_qtree>& tksem,
                     vector<long>& extendsto,
                     vector<bool>& ignore,
                     tdopout_struct*    tdo
                     ) 
    : _tk(tk),
      _tkend(tkend),
      _tky(tky),
      _tksem(tksem),
      _extendsto(extendsto),
      _ignore(ignore),
      _tdo(tdo),
      _next(0),
      _accumulated_parsed_qtree(0),
      _token(0),
      _eof(false)
    ,_incomplete(false)
{
    reset();
}


qtree*  qtreefactory::make_node(long id, int lbp) {

    if (id >= _tksz || id < 0) return 0;
    
    if (lbp < 0) lbp = 0;

    oid2qtree_it it = _symtable.find(id);
    qtree* s = 0;
    
    if (it != _symtable.end()) {
        s = it->second;
        
        if (lbp > s->_lbp) {
            s->_lbp = lbp;
        }
        
    } else {
        // get a pointer to our new_{type} function
        ptr2new_qtree ctor = _tksem[id];
        assert(ctor);
        qqchar vnew(_tk[id], _tkend[id]);

        // if we have one, call it.
        if (ctor) {
            s = ctor(id,vnew);
        } else {
            // probably need to augment the quicktype.cpp table with another ctor.
            assert(0);

            // put in an atom  - not good enough 
            s = new_t_ato(id,vnew);
        }
        assert(s);
        s->set_qfac(this);

        long ext = _extendsto[id];
        if (ext > -1) {
            qqchar ender( _tk[ext], _tkend[ext]);            
            s->set_bookend(ender);
        }

        s->_type = t_qtr;
        s->_owner = _tdo->_myobj->_mytag;

        s->_ser = serialfactory->serialnum_sxp(s, s->_owner);
        s->_owner->sexpstack_push(s);

        s->_reserved = 0;
        s->_malloc_size = sizeof(qtree);


#if 0          // some of our reserved words (try, catch, throw), we just want to treat as atoms for now.

        if (s->_ty) {
            assert(s->_ty == _tky[id]);
        }
        s->_ty = _tky[id];

#endif


        l3path varname(0,"qtree_node_of_%s_at_%p", _tky[id], s);
        l3path msg(0,"777777 %p new qtree node '%s'  @serialnum:%ld\n",s,varname(),s->_ser);
        DV(printf("%s\n",msg()));
        MLOG_ADD(msg());


        _symtable.insert(hashmap_oid2qtree::value_type(id, s));

        if (s) {
            if (lbp > s->_lbp) {
                s->_lbp = lbp;
            }
        } 
    }
    return s;
}

long tk_lineno(long errtok, 
               vector<t_typ>& _tky
               ) {
    
    // find the line number
    long line =0;
    for (long i =0; i < errtok; ++i) {
        if (_tky[i] == t_newline) ++line;
    }
    
    return line;
}


void tk_print_error_context(long errtok, 
                            const char* filename,
                            vector<char*>& _tk, 
                            vector<char*>& _tkend, 
                            vector<t_typ>& _tky
                            ) {

    long _tksz = _tk.size();

    long line = tk_lineno(errtok, _tky);

    long colnum = 0;

    // find the enclosing newlines (or bof/eof)

    if (_tky[errtok] == t_newline) {
        printf("error in qtreefactory::print_error_context: a newline cannot be the location of an errtok.\n");
        l3throw(XABORT_TO_TOPLEVEL);

        assert(0);
        exit(1);
    }

    long  b = errtok;
    while(b > 0 && _tky[b] != t_newline) --b;
    char* beg = _tk[b];

    long  e = errtok;
    long  valid = _tksz-1;
    while(e < valid && _tky[e] != t_newline) ++e;
    char* end = 0;
    if (e < valid) {
        end = _tkend[e-1]; // don't include the newline, b/c might just be eof
    } else {
        end = _tkend[e];
    }



    int   l1 = end-beg;
    int   linesz = l1 + 1; // +1 for newline
    int   l2 = linesz + linesz +1; // +1 for trailing '\0'
    char* pt = (char*)::malloc( l2);
    memset(pt,' ',l2);
    pt[l2-1]=0;
    pt[l2-2]='\n';
    pt[ linesz + (_tk[errtok] - beg) ] = '^';

    strncpy(pt, beg, end-beg);
    pt[l1]='\n';

    colnum = (_tk[errtok] - beg) + 1;
    printf("at file '%s': line %ld  : col %ld\n", filename, line+1, colnum);
    printf("%s\n",pt);
    ::free(pt);
}

void qtreefactory::print_error_context(long errtok) {

    tk_print_error_context(errtok, _tdo->filename(errtok), _tk, _tkend, _tky);

}


// globals. advance changes _token, inside qtfac.
// qtree* token = 0;
// qtreefactory* qtfac = 0;


t_typ qtreefactory::advance(t_typ expected) {

    // sanity check that _tksz has been brought up to date.
    assert((unsigned long)_tksz == _tk.size());

    _cur = _next;

    if (expected) {
        if (!_token) {
            printf("error in qtreefactory::advance(): expected '%s' but _token was null.\n",expected);
            l3throw(XABORT_TO_TOPLEVEL);
        }

        t_typ saw = _token->_ty;
        if (saw != expected) {
            printf("error in parsing during advance: expected type '%s' but saw type '%s'.\n",
                   expected, saw);
            print_error_context(_token->_id);
            l3throw(XABORT_TO_TOPLEVEL);
        }
    }

    // get to a good token (not ignored, not a newline) or get to eof.
    while (_cur < _tksz) {
        if (_ignore[_cur]) { ++_cur; continue; }
        if (_tky[_cur] == t_newline) { ++_cur; continue; }
        break;
    }

    if (_cur >= _tksz-1) {
        // at EOF, or at least as much as the user typed so far.
        assert(_tky[_cur] == t_eof);

        // if we are inside a nested (, {, or [, definitely request more input.
        if (_extendsto[_cur] < -1) {
            DV(printf("*&* _extendsto[_cur] == %ld ... requesting more input.\n", _extendsto[_cur]));
            request_more_input(_parcmd);
        }
    }


    if (_cur >= _tksz) {
        _next = _cur + 1;
        _eof = true;
        _token = 0;
        return 0;
    }

    _token = make_node(_cur,-1);

    // important: update next in case _cur got advanced
    _next = _cur +1;

    return _token->_ty;
}




// working expression() parsing routine. Reproduces the original Pratt and Crockford formulation.

//
// _left holds the accumulated parse tree at any point in time.
//     "The parse Tree Up to this point, by consuming the tokens to the left" would be a better-but-too-long name.
//
//  and... _left obviously is the stuff to the left of the current operator, hence the name.
//
// data flows from token -> t -> (possibly on the stack of t recursive munch_left calls) -> into the _left tree.
//
//  better names: _left  -> _accumulated_parsed_qtree (to be returned / apqt)
//                t      -> cnode  (the current token's qtree node to be processed. Once we grab this we always advance() past it
//                                     before processing it, so that _global_next_token (or _token) contains the following token.
//
//                  actually we just keep '_token' as in, rather than this more verbose description we considered:
//                _token -> _global_next_token_node (since it is a global variable and can/will be incremented by many routines)
//
//
//  meaning of rbp parameter: if you hit a token with a  _token->_lbp < rbp, then don't bother calling munch_left, stop and return what you have.
//
// better explanation:  rbp = a lower bound on descendant nodes precedence level, so we can 
//    guarantee the precenence-hill-climbing property (small precedence at the top) in the resulting 
//    parse tree.
//
qtree* qtreefactory::expression(int rbp) {

    DV(printf("_cnode_stack has size %ld:\n", _cnode_stack.size()));
    DV(
       std::list<qtree*>::iterator be = _cnode_stack.begin();
       std::list<qtree*>::iterator en = _cnode_stack.end();
       long i = 0;
       for (; be != en; ++be, ++i) {
            printf("\n*** _cnode_stack[%02ld] = \n",i);
            (*be)->p();
        }
        printf("\n");
       );

    DV(symbol_print(_token,"_token: "));

    qtree* cnode = _token;

    if (eof()) {
        return cnode;
    }
    _cnode_stack.push_front(_token);

    advance(0);

    assert(_token);
    DV(_token->print("_token: "));

    if (cnode) {

        // munch_right() of atoms returns this/itself, in which case: _accumulated_parsed_qtree = t; is the result.
        _accumulated_parsed_qtree = cnode->munch_right(cnode);
        DV(_accumulated_parsed_qtree->print("_accumulated_parsed_qtree: "));
    }

    while(!eof() && rbp < _token->_lbp ) {
        assert(_token);

        cnode = _token;
        _cnode_stack.front() = _token;

        DV(cnode->print("cnode:  "));

        advance(0);
        if (_token) {
            DV(_token->print("_token:  "));
        }

        // if cnode->munch_left() returns this/itself, then the net effect is: _accumulated_parsed_qtree = cnode;
        _accumulated_parsed_qtree = cnode->munch_left(cnode, _accumulated_parsed_qtree);

    }

    _cnode_stack.pop_front();
    return _accumulated_parsed_qtree;
}

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////
//
// qtree methods



qtree* base_munch_right(qtree* _this_) {
    return _this_;
}

qtree* base_munch_left(qtree* _this_, qtree* left) {
    return _this_;
}



void qtree::sexp_string(l3bigstr* d, const char* ind) {
        assert(d);

        l3path inmore;

        bool opened = false;
        if (_headnode || _chld.size() || _body) {
            d->pushf("(");
            opened = true;
        }


        l3path val;
        val.p += addlexed(_val.b, _val.e, &(val.p));

        d->pushf("%s",val());

        if (_headnode) {
            d->pushf(" ");
            _headnode->sexp_string(d,ind ? inmore() : 0);
        }

        int nc = _chld.size();

        for (int i = 0; i < nc; ++i) {
            d->pushf(" ");
            _chld[i]->sexp_string(d,ind ? inmore() : 0);
        }

        if (_body) {
            d->pushf(" ");
            _body->sexp_string(d,ind ? inmore() : 0);
        }

        if (opened) {
            d->pushf(")");
        }
}




qtree*  new_t_ato(long id, const qqchar& v) { return new qtree_t_ato(id,v); }

qtree*  new_number(long id, const qqchar& v) { return new qtree_number(id,v); }
qtree*  new_t_dou(long id, const qqchar& v) { return new qtree_t_dou(id,v); }

//qtree*  new_qtree_base(long id, const qqchar& v) { return new qtree(id,v); }
qtree*  new_qtree_infix(long id, const qqchar& v) { return new qtree_infix(id,v); }
qtree*  new_qtree_prefix(long id, const qqchar& v) { return new qtree_prefix(id,v); }
qtree*  new_qtree_postfix(long id, const qqchar& v) { return new qtree_postfix(id,v); }


qtree* new_t_newline(long id, const qqchar& v) { return 0; } // no token for newlines!



#define DEFINE_QTREE_NEW(type,prec) qtree*  new_##type(long id, const qqchar& v) {  return new qtree_##type(id,v); }


DEFINE_QTREE_NEW(t_cppcomment,0)

DEFINE_QTREE_NEW(t_vec,10)

DEFINE_QTREE_NEW(m_bng,70)
DEFINE_QTREE_NEW(m_tld,70)
DEFINE_QTREE_NEW(t_lsp,100)
DEFINE_QTREE_NEW(t_lsc,100)

DEFINE_QTREE_NEW(m_eql,40)
DEFINE_QTREE_NEW(m_eqe,40)
DEFINE_QTREE_NEW(m_lan,30)
DEFINE_QTREE_NEW(m_lor,30)

// bit wise or |  and bit wise and &
DEFINE_QTREE_NEW(t_and,30)
DEFINE_QTREE_NEW(m_bar,30)


DEFINE_QTREE_NEW(m_neq,40)
//DEFINE_QTREE_NEW(m_bng,10)
DEFINE_QTREE_NEW(m_mlt,10)
DEFINE_QTREE_NEW(m_crt,10)
DEFINE_QTREE_NEW(m_dvn,10)
DEFINE_QTREE_NEW(m_mod,10)
DEFINE_QTREE_NEW(m_pls,10)
DEFINE_QTREE_NEW(m_mns,10)
DEFINE_QTREE_NEW(m_gth,40)
DEFINE_QTREE_NEW(m_lth,40)
DEFINE_QTREE_NEW(m_gte,40)
DEFINE_QTREE_NEW(m_lte,40)
DEFINE_QTREE_NEW(m_peq,10)
DEFINE_QTREE_NEW(m_meq,10)
DEFINE_QTREE_NEW(m_dve,10)
DEFINE_QTREE_NEW(m_mte,10)
DEFINE_QTREE_NEW(m_cre,10)
DEFINE_QTREE_NEW(m_umi,10)

DEFINE_QTREE_NEW(m_ats,10)
DEFINE_QTREE_NEW(m_hsh,10)
DEFINE_QTREE_NEW(m_dlr,10)
DEFINE_QTREE_NEW(m_pct,10)
DEFINE_QTREE_NEW(m_cln,10)
DEFINE_QTREE_NEW(m_que,10)

DEFINE_QTREE_NEW(m_ppl,10)
DEFINE_QTREE_NEW(m_mmn,10)



// protobuf keywords

DEFINE_QTREE_NEW(t_pb_message,10)
DEFINE_QTREE_NEW(t_pb_import,10)
DEFINE_QTREE_NEW(t_pb_package,10)
DEFINE_QTREE_NEW(t_pb_service,10)
DEFINE_QTREE_NEW(t_pb_rpc,10)
DEFINE_QTREE_NEW(t_pb_extensions,10)
DEFINE_QTREE_NEW(t_pb_extend,10)
DEFINE_QTREE_NEW(t_pb_option,10)
DEFINE_QTREE_NEW(t_pb_required,10)
DEFINE_QTREE_NEW(t_pb_repeated,10)
DEFINE_QTREE_NEW(t_pb_optional,10)
DEFINE_QTREE_NEW(t_pb_enum,10)


DEFINE_QTREE_NEW(t_pby_int32,10)
DEFINE_QTREE_NEW(t_pby_int64,10)
DEFINE_QTREE_NEW(t_pby_uint32,10)
DEFINE_QTREE_NEW(t_pby_uint64,10)
DEFINE_QTREE_NEW(t_pby_sint32,10)
DEFINE_QTREE_NEW(t_pby_sint64,10)
DEFINE_QTREE_NEW(t_pby_fixed32,10)
DEFINE_QTREE_NEW(t_pby_fixed64,10)
DEFINE_QTREE_NEW(t_pby_sfixed32,10)
DEFINE_QTREE_NEW(t_pby_sfixed64,10)
DEFINE_QTREE_NEW(t_pby_string,10)
DEFINE_QTREE_NEW(t_pby_bytes,10)


// ; \n // , 

DEFINE_QTREE_NEW(t_semicolon,0)

DEFINE_QTREE_NEW(t_comma,0)

// = ( ) { } [ ]

DEFINE_QTREE_NEW(t_asi,10)

DEFINE_QTREE_NEW(t_opn,100)
DEFINE_QTREE_NEW(t_cpn,0)

DEFINE_QTREE_NEW(t_obr,10)
DEFINE_QTREE_NEW(t_cbr,10)

DEFINE_QTREE_NEW(t_csq,10)

DEFINE_QTREE_NEW(t_ut8,0)
DEFINE_QTREE_NEW(t_sqo,110)  // single quote
DEFINE_QTREE_NEW(t_q3q,110)
DEFINE_QTREE_NEW(t_chr,0)


    //DEFINE_QTREE_NEW(t_dou,0)
DEFINE_QTREE_NEW(t_oso,10)
DEFINE_QTREE_NEW(t_ico,10)
DEFINE_QTREE_NEW(t_iso,10)
DEFINE_QTREE_NEW(t_oco,10)

// C prefix words:
DEFINE_QTREE_NEW(t_c_auto,0)
DEFINE_QTREE_NEW(t_c_double,70)
DEFINE_QTREE_NEW(t_c_int,0)
DEFINE_QTREE_NEW(t_c_char,0)
DEFINE_QTREE_NEW(t_c_extern,0)
DEFINE_QTREE_NEW(t_c_const,0)
DEFINE_QTREE_NEW(t_c_float,0)
DEFINE_QTREE_NEW(t_c_short,0)
DEFINE_QTREE_NEW(t_c_unsigned,0)
DEFINE_QTREE_NEW(t_c_signed,0)
DEFINE_QTREE_NEW(t_c__Bool,0)
DEFINE_QTREE_NEW(t_c_inline,0)
DEFINE_QTREE_NEW(t_c__Complex,0)
DEFINE_QTREE_NEW(t_c_restrict,0)
DEFINE_QTREE_NEW(t_c__Imaginary,0)
DEFINE_QTREE_NEW(t_c_volatile,0)

// C++ prefix words
DEFINE_QTREE_NEW(t_cc_bool,0)
DEFINE_QTREE_NEW(t_cc_explicit,0)
DEFINE_QTREE_NEW(t_cc_virtual,0)
DEFINE_QTREE_NEW(t_cc_mutable,0)
DEFINE_QTREE_NEW(t_cc_protected,0)
DEFINE_QTREE_NEW(t_cc_wchar_t,0)


DEFINE_QTREE_NEW(t_c_struct,0)
DEFINE_QTREE_NEW(t_c_break,0)
DEFINE_QTREE_NEW(t_c_else,0)
DEFINE_QTREE_NEW(t_c_long,0)
DEFINE_QTREE_NEW(t_c_switch,0)
DEFINE_QTREE_NEW(t_c_case,0)
//DEFINE_QTREE_NEW(t_c_enum,0) // already in protobuf words above
DEFINE_QTREE_NEW(t_c_register,0)
DEFINE_QTREE_NEW(t_c_typedef,0)
DEFINE_QTREE_NEW(t_c_return,0)
DEFINE_QTREE_NEW(t_c_union,0)
DEFINE_QTREE_NEW(t_c_continue,0)
DEFINE_QTREE_NEW(t_c_for,0)
DEFINE_QTREE_NEW(t_c_void,0)
DEFINE_QTREE_NEW(t_c_default,0)
DEFINE_QTREE_NEW(t_c_goto,0)
DEFINE_QTREE_NEW(t_c_sizeof,0)
DEFINE_QTREE_NEW(t_c_do,0)
DEFINE_QTREE_NEW(t_c_if,0)
DEFINE_QTREE_NEW(t_c_static,0)
DEFINE_QTREE_NEW(t_c_while,0)
DEFINE_QTREE_NEW(t_cc_asm,0)
DEFINE_QTREE_NEW(t_cc_dynamic_cast,0)
DEFINE_QTREE_NEW(t_cc_namespace,0)
DEFINE_QTREE_NEW(t_cc_reinterpret_cast,0)
DEFINE_QTREE_NEW(t_cc_new,0)
DEFINE_QTREE_NEW(t_cc_static_cast,0)
DEFINE_QTREE_NEW(t_cc_typeid,0)

// let these be atoms, for now.
DEFINE_QTREE_NEW(t_cc_try,0)
DEFINE_QTREE_NEW(t_cc_catch,0)
DEFINE_QTREE_NEW(t_cc_throw,0)

DEFINE_QTREE_NEW(t_cc_false,0)
DEFINE_QTREE_NEW(t_cc_operator,0)
DEFINE_QTREE_NEW(t_cc_template,0)
DEFINE_QTREE_NEW(t_cc_typename,0)
DEFINE_QTREE_NEW(t_cc_class,0)
DEFINE_QTREE_NEW(t_cc_friend,0)
DEFINE_QTREE_NEW(t_cc_private,0)
DEFINE_QTREE_NEW(t_cc_this,0)
DEFINE_QTREE_NEW(t_cc_using,0)
DEFINE_QTREE_NEW(t_cc_const_cast,0)

DEFINE_QTREE_NEW(t_cc_public,0)
DEFINE_QTREE_NEW(t_cc_delete,0)
DEFINE_QTREE_NEW(t_cc_true,0)
DEFINE_QTREE_NEW(t_cc_and,0)
DEFINE_QTREE_NEW(t_cc_bitand,0)
DEFINE_QTREE_NEW(t_cc_compl,0)
DEFINE_QTREE_NEW(t_cc_not_eq,0)
DEFINE_QTREE_NEW(t_cc_or_eq,0)
DEFINE_QTREE_NEW(t_cc_xor_eq,0)
DEFINE_QTREE_NEW(t_cc_and_eq,0)
DEFINE_QTREE_NEW(t_cc_bitor,0)
DEFINE_QTREE_NEW(t_cc_not,0)
DEFINE_QTREE_NEW(t_cc_or,0)
DEFINE_QTREE_NEW(t_cc_xor,0)


DEFINE_QTREE_NEW(t_any,0)
DEFINE_QTREE_NEW(t_eof,0)

// variable declaration
DEFINE_QTREE_NEW(t_dcl,0)

//qtree*  new_t_osq(long id, const qqchar& v) { return new qtree_t_osq(id,v); }
DEFINE_QTREE_NEW(t_osq,0)

DEFINE_QTREE_NEW(t_cmd,10)

DEFINE_QTREE_NEW(t_ref,0)

// do some parsing, store the result



L3METHOD(tdop_dtor)
{
    assert(obj->_type == t_tdo);

    tdopout_struct* tdop1 = (tdopout_struct*)(obj->_pdb);
   
    tdop1->~tdopout_struct();
}
L3END(tdop_dtor)



//
// during cp we also move everything into strdup and not mmap files.
//
L3METHOD(tdop_cpctor)
{

    LIVEO(obj);
    l3obj* src  = obj;     // template
    l3obj* nobj = *retval; // write to this object
    assert(src);
    assert(nobj); // already allocated, named, _owner set, etc. See objects.cpp: deep_copy_obj()
    
    Tag* tdop_tag = nobj->_mytag;

    // assert we are a captag pair
    assert( tdop_tag );
    assert( pred_is_captag_obj(nobj) );
    
    tdopout_struct* tdop_src = (tdopout_struct*)(src->_pdb);
    tdopout_struct* tdop1    = (tdopout_struct*)(nobj->_pdb);

    // call constructor first...using placement new with copy constructor invocation
    tdop1 = new(tdop1) tdopout_struct(*tdop_src);
    
    // and the one piece that CopyFrom can't do for us.
    tdop1->_myobj = nobj;

    // *retval was already set for us, by the base copy ctor.
}
L3END(tdop_cpctor)


//
// if obj  !=0: take string from (qqchar*)obj
// else 
// if sexp !=0: (tdop infix_text_to_parse)
//
// if supplied, curfo = parse_cmd*, which has fields thus:
//
//     ustaq<sexp_t>* _rstk; // result ustaq, optional.
// 
//     bool _partial_queueit;      // policy for this call
//     bool _partial_throw;        // policy for this call
//
//     long _nchar_consumed;     // results of this call
//     bool _was_incomplete;     // results of this call
//
//
L3METHOD(tdop) 
{
   LIVET(retown);

    l3obj* vv  = 0;
    parse_cmd  default_cmd;
    parse_cmd* cmd = &default_cmd;

    if (curfo) {
        parse_cmd*  pc = (parse_cmd*)curfo;
        assert(pc->_type == t_pcd);
        cmd = pc;
    }

    qqchar* pqqstart = (qqchar*)obj;

    l3path p;
    tdopout_struct* tdop1 = 0;

        //
        // for constructing our tdopout_struct, we set these:
        //
        char*        start = 0;
        const char*  fname = "(internal string)";
        bool         start_is_mmap = false;
        long         start_mapsz =0;
                
        l3obj* nobj = 0;
        l3path clsname("tdop");
        l3path objname("tdop");
        long extra = sizeof(tdopout_struct);


    XTRY
        case XCODE:

        if (obj) {

            // for command line parsing, tdop no longer owns it's memory, permatext does:
            //p.reinit(*pqqstart);
            //start = strdup(p());
            //start_mapsz = p.len();
            // instead:

            start = pqqstart->b; // so just point to the permatext.
            start_mapsz = -(pqqstart->len()) ; // and signal it should be ignored in dtor with negative length.

        } else {
            
            k_arg_op(0,1,exp,env,&vv,owner,curfo,etyp,retown,ifp);

            l3obj* infix_text_to_parse = 0;
            ptrvec_get(vv,0,&infix_text_to_parse);
            
            if (infix_text_to_parse->_type != t_str) {
                printf("error in tdop: the argument supplying the infix text to parse was not a string.\n");
                l3throw(XABORT_TO_TOPLEVEL);     
            }
            
            string_get(infix_text_to_parse,0,&p);
            
            if (p.len()==0) {
                printf("error in tdop: the infix text to parse was of length 0.\n");
                l3throw(XABORT_TO_TOPLEVEL);
            }
            
            start = strdup(p());
            start_mapsz = p.len();
        }

        make_new_captag((l3obj*)&objname, extra,exp,   env,&nobj,owner,   (l3obj*)&clsname,t_cap,retown,ifp);
        
        nobj->_type = t_tdo;
        nobj->_parent_env  = env;
        
        tdop1 = (tdopout_struct*)(nobj->_pdb);
        tdop1 = new(tdop1) tdopout_struct(start,fname,start_is_mmap,start_mapsz); // placement new

        tdop1->_myobj = nobj;
        
        nobj->_dtor = &tdop_dtor;
        nobj->_cpctor = &tdop_cpctor;
        
        *retval = nobj;
        
        // and do the lexing and parsing...
        tdop1->lex_and_parse(cmd);
        
   break;
   case XFINALLY:
     if (vv) { generic_delete(vv, L3STDARGS_OBJONLY); }
     break;
     XENDX

}
L3END(tdop)




tdopout_struct::~tdopout_struct() {

    long N = _vstart.size();

    for (long i = 0; i < N; ++i ) {

        if (_vfd[i] != -1) {
            close(_vfd[i]);
        }
        
        if (_vis_mmap[i]) {
            assert(_vmapsz[i] > 0);
            munmap(_vstart[i], _vmapsz[i]);

        } else if (_vstart[i]) {

            // we flag permatext owned source text with negative (<0) _vmapsz.
            if(_vmapsz[i] >= 0) {            
                ::free(_vstart[i]);
            }
        }
    }
}



void tdopout_struct::init_mmap_from_file() {
    
    _vis_mmap[_k] = false; // set to true if we complete this routine.

    file_ty ty = FILE_DOES_NOT_EXIST;
    long sz = 0;
    bool exists = file_exists(_vfname[_k].c_str(), &ty, &sz);
    
    if (!exists || (ty != REGFILE && ty != SYMLINK)) {
        printf("error in tdop: got bad file name '%s': not a readable file.\n", _vfname[_k].c_str());
        l3throw(XABORT_TO_TOPLEVEL);
    }

    if (sz == 0) {
        printf("error in tdop: got empty file '%s'.\n", _vfname[_k].c_str());
        l3throw(XABORT_TO_TOPLEVEL);
    }
    _vmapsz[_k] = sz;

    _vfd[_k] = open(_vfname[_k].c_str(), O_RDONLY | O_RDWR);
    if (-1 == _vfd[_k]) {
        l3path msg(0,"error in tdop: open of file '%s' failed", _vfname[_k].c_str());
        perror(msg());
        l3throw(XABORT_TO_TOPLEVEL);
    }

    _vstart[_k] = (char*)mmap(0,  // addr
                  _vmapsz[_k], // len
                  PROT_READ | PROT_WRITE, //prot 
                  MAP_FILE  | MAP_SHARED,  // flags
                  _vfd[_k], // fd
                  0   // offset
                  );
    
    if (_vstart[_k] == MAP_FAILED) {
        l3path msg(0,"error in tdop: mmap failed for .proto file '%s'",_vfname[_k].c_str());
        perror(msg());
        l3throw(XABORT_TO_TOPLEVEL);
    }

    _vis_mmap[_k] = true;
}



void tdopout_struct::backup_one_elim_eof() {

    // here, I don't think the qtree_t_eof never got instantiated, so don't delete it.
    
    DV(_qf.dump_symtable());
    
    ulong N = _tk.size();
    if (!N) return;
    // and backup, eliminating all vestiges of the t_eof.
    assert(_tky[N-1] == t_eof);
    
    // take off the t_eof at the end of the arrays.
    _tk.pop_back();
    _tkend.pop_back();
    _tky.pop_back();
    
    _tksem.pop_back();
    _extendsto.pop_back();
    _ignore.pop_back();
    
    //    --_firstid.back();

}



void tdopout_struct::backup_one_elim_eof_for_addstring() {

         DV(_qf.dump_symtable());

         ulong N = _tk.size();
         if (!N) return;

         // gotta delete the old eof qtree* node, already instantiated...
         oid2qtree_it it = _qf._symtable.find(N-1);         
         if (it == _qf._symtable.end()) {
             printf("internal error in tdopout_struct::backup_one_elim_eof(): could not find old t_eof qtree* node when extending parse.\n");
             assert(0);
             exit(1);
         } 

         // and backup, eliminating all vestiges of the t_eof.
             assert(_tky[N-1] == t_eof);

             _qf.delete_id(N-1);  // does the destroy_sexp() for us.
 
             // take off the t_eof at the end of the arrays.
             _tk.pop_back();
             _tkend.pop_back();
             _tky.pop_back();

             _tksem.pop_back();
             _extendsto.pop_back();
             _ignore.pop_back();

             //             --_firstid.back(); // is this right???

}


//
//
//
long tdopout_struct::add_string_segment(const char* p, long sz) {

         _vstart.push_back(strdup(p));
         _vfname.push_back("(internal string)");
         _vmapsz.push_back(sz);
         _vis_mmap.push_back(false);
         _vfd.push_back(-1);
         _firstid.push_back(_tk.size());

         backup_one_elim_eof_for_addstring();

         _k++;
         // sanity check we are correct with _k setting
         assert(_vstart.size() - 1 == (ulong)_k);

         parse_cmd cmd;
         cmd._forbid_calling_get_more_input = true;
         return lex_and_parse(&cmd);
 }


void tdopout_struct::update_extendsto_and_ignore() {

    _extendsto.resize(_tk.size(),-1); // holds paren matches, and where comments pick up again.
    _ignore.resize(_tk.size(),false);     // true if in a comment, else false

    // gotta be sure to reset all these
    for (ulong i =0; i < _tk.size(); ++i) {
        _ignore[i] = false;
        _extendsto[i]=-1;
    }

    // -1) ignore cpp comments
    ignore_cpp_comments(_vfname[_k].c_str(), _vstart[_k], 0, _vmapsz[_k], _tk, _tkend, _tky, _tksem, _extendsto, _ignore, _partialokay);


    // 0) match triple quotes """ first, so we can also set ignore for the rest of the matchers!
    match_triple_quotes(_vfname[_k].c_str(), _vstart[_k], 0, _vmapsz[_k], _tk, _tkend, _tky, _tksem, _extendsto, _ignore, _partialokay);


    // .5) and single quotes
    mark_within_single_quotes_as_ignored(_vfname[_k].c_str(), _vstart[_k], 0, _vmapsz[_k], _tk, _tkend, _tky, _tksem, _extendsto, _ignore, _partialokay);


    DV(printf("after matching triple quotes:\n"));
    DV(dumplisp(_vstart[_k], 0, _tk, _tkend, _tky, _extendsto, _ignore));


    // 1) compute_extends_to also marks cpp comments in _ignore.
    // 
    compute_extends_to(_vfname[_k].c_str(), _vstart[_k], 0, _vmapsz[_k], _tk, _tkend, _tky, _tksem, _extendsto, _ignore, _partialokay);


    // 2) match {}
    match_braces(_vfname[_k].c_str(), _vstart[_k], 0, _vmapsz[_k], _tk, _tkend, _tky, _tksem, _extendsto, _ignore, _partialokay);
    
    DV(printf("after matching braces:\n"));
    DV(dumplisp(_vstart[_k], 0, _tk, _tkend, _tky, _extendsto, _ignore));
        
    // 3) match []
    match_square_brackets(_vfname[_k].c_str(), _vstart[_k], 0, _vmapsz[_k], _tk, _tkend, _tky, _tksem, _extendsto, _ignore, _partialokay);
    
    DV(printf("after matching square brackets:\n"));
    DV(dumplisp(_vstart[_k], 0, _tk, _tkend, _tky, _extendsto, _ignore));


}

//
// return the number of char parsed, so we know where to start parsing next, and push the sexp_t* onto stk
//
long tdopout_struct::lex_and_parse(parse_cmd* client_parse_cmd) {

    // tell _qf about cmd, so it knows where to go for more input.
    assert(client_parse_cmd);
    _qf.set_parse_cmd(*client_parse_cmd);

     //
     // step 0: lex into our _tk* arrays
     //
    lex((char*)_vstart[_k], _vmapsz[_k], _tk, _tkend, _tky, _tksem, client_parse_cmd->_filename);

    //    printf("dump is:\n");
    //    dump(tk, tkend, tky);
    
    DV(
       printf("my tk is:\n");
       dumpta(_tk, _tkend, _tky);    
       )

    update_extendsto_and_ignore();

    DV(
       printf("after update_extendsto_and_ignore():\n");
       dumpta(_tk, _tkend, _tky, &_extendsto, &_ignore, &_qf));

    return  and_parse(client_parse_cmd);

 }


void tdopout_struct::save_expr(qtree* psd, parse_cmd* client_parse_cmd) {

    _parsed_ustaq.push_back(psd);
    if (client_parse_cmd && client_parse_cmd->_rstk) {
        client_parse_cmd->_rstk->push_back(psd);
    }

    // 5) and save the viewable / testable parse tree:
    l3bigstr d;
    psd->stringify(&d,0);
    if (_output_stringified_compressed.size()) {
            _output_stringified_compressed += " ";
    }
    _output_stringified_compressed += d();
    
    d.clear();
    psd->stringify(&d,"");
    if (_output_stringified_viewable.size()) {
        _output_stringified_viewable += " ";
    }
    _output_stringified_viewable += d();


    // last, seal it so that it becomes independet of _qfac
    psd->seal();
    
}




//
// isolate just the "and parse" of "lex_and_parse", so that CopyFrom can use it without repeating the lexing.
//
// return the number of characters consumed, so we know where to start parsing next.
//
// add parsed sexp_t* to client_parse_cmd._rstk as well as our intnernal _parsed_ustaq
//
long tdopout_struct::and_parse(parse_cmd*  client_parse_cmd) {
    assert(client_parse_cmd);

   // 4) and parse... re-parse everything from the start.

    // since we are restarting, loose any old stored...
    _parsed_ustaq.clear();

    char* orig_start        = _vstart[_k];
    char* expression_start  = _vstart[_k];
    long  starting_segment  = _k;

    _qf.reset();
    _qf.advance(0);

    while(!_qf.eof()) {
        // save where we started so we can consume nchar consumed:
        if (_qf.curidx() <= 0) {
            expression_start = _tk[0];
        } else {
            expression_start = _tkend[_qf.curidx()-1];
        }            

        //
        // the main parsing call to expression(0) is here:
        //
        //    the argument is the lower bound on precedence below us, and 
        //    needs to be zero or greater, 0 here to stop after parsing one tree.
        //
        _parsed = _qf.expression(0); 

        if (!_parsed) {
            printf("error in tdop -- lex_and_parse got back null parse tree.\n");
            l3throw(XABORT_TO_TOPLEVEL);
        }

        //        if (!_qf.incomplete()) {
        if (true) {
            // complete parse obtained.
            save_expr(_parsed, client_parse_cmd);

            client_parse_cmd->_was_incomplete = false;
            client_parse_cmd->_nchar_consumed = (_tk[_qf.curidx()] - orig_start);

        } else {  // parse was incomplete

            client_parse_cmd->_was_incomplete = true;
            client_parse_cmd->_nchar_consumed = (expression_start - orig_start);
            

            if (client_parse_cmd->_partial_throw) {
                printf("error in tdop::lex_and_parse() found incomplete parse, throwing on cmd->_partial_throw policy.\n");
                l3throw(XABORT_TO_TOPLEVEL);
            }

        
            if (client_parse_cmd->_partial_queueit) {  
                save_expr(_parsed, client_parse_cmd);

            } else {

                // don't queue it or return it;
                _parsed = 0;

            }
        }
        
    } // end  while !eof


    if (_k == starting_segment) {
        // okay
    } else {
        printf("unfinished: how to compute ngone correctly / return nchar consumed when we cross multiple segments???\n");
        assert(0); 
        exit(1);
        return -1;
    }

    return client_parse_cmd->_nchar_consumed;
}


void tdo_print(l3obj* obj, const char* indent, stopset* stoppers) {
     assert(obj);
     assert(obj->_type == t_tdo);

     tdopout_struct* tdop1 = (tdopout_struct*)(obj->_pdb);
  
     l3path indent_more(indent);
     indent_more.pushf("%s","     ");

     if (tdop1->_parsed == 0) {
         printf("%s\"\"\n",indent_more());
         return;
     }

     l3bigstr d;
     tdop1->_parsed->stringify(&d,indent_more());
     d.outln();

     printf("\n from the original:\n");
     tdop1->print_orig();

     printf("\n and as sexp:\n");
     tdop1->print_sexp();

     // mostly here to make sure that p() gets instantiated for gdb.
     DV(tdop1->_parsed->p());
}

 qtree* tdop_get_parsed(l3obj* obj) {
     assert(obj);
     assert(obj->_type == t_tdo);

     tdopout_struct* tdop1 = (tdopout_struct*)(obj->_pdb);
     //     return tdop1->_parsed;
     return tdop1->_parsed_ustaq.front_val(); // return the next in FIFO order, whereas _parsed has the last seen.
 }



sexp_t *copy_sexp(sexp_t *s, Tag* ptag) {

    LIVET(ptag);
    LIVEO(s);

    // gotta be already fully done with construction, before we can copy:
    assert(is_sealed(s));

    // use qtree's copy ctor
    qtree* newnode = new qtree(*s);

    // and then shift ownership
    transfer_subtree_to(newnode, ptag);

    return newnode;
}






bool assert_sexp_all_owned_by_tag(qtree* exp, Tag* ptag) {

    if (exp == NULL) return true;
    LIVET(ptag);

    assert(exp->_owner == ptag);

     if (exp->_body) {
         assert_sexp_all_owned_by_tag(exp->_body, ptag);
     }
     
     if (exp->_headnode) { 
         assert_sexp_all_owned_by_tag(exp->_headnode, ptag);
     }
     
     ulong nc = exp->_chld.size();
     for (ulong i =0 ; i < nc; ++i) {
         assert_sexp_all_owned_by_tag(exp->_chld[i], ptag);
     }    

    return true;
}

//
// called by Tag at cleanup time.
//
void destroy_sexp(sexp_t* s) {

     // confirm sanity.
     LIVEO(s);

     DV(printf("called destroy_sexp on sexp_t %p:\n",s));
     DV(s->print("in destroy_sexp:    "));

     assert(s->_owner->sexpstack_exists(s));
     s->_owner->sexpstack_remove(s);

     long sn_saw = s->_ser;
     l3path name_freed;
     long sn_del = serialfactory->del(s,&name_freed);
     assert(sn_del == sn_saw);
     
     l3path msg(0,"888888 %p delete qtree* '%s' @serialnum:%ld\n",s,name_freed(),sn_del);
     DV(printf("%s\n",msg()));
     MLOG_ADD(msg());
     
     delete s;
 }

//
// recursively_destroy_sexp(sx, tag)
//
// selectively delete just sx (and descendants) that belong to tag.
//
//  we stop if we hit a node not owned by tag. This allows entire subtrees
//   to be transfered away without risking them being destroyed here.
//
void  recursively_destroy_sexp(qtree* sx, Tag* tag) {
    assert(sx);
    LIVEO(sx);
    LIVET(tag);

    if (sx->_owner != tag) return;

    if (sx->_headnode) {
        recursively_destroy_sexp(sx->_headnode, tag);
    }

    if (sx->_body) {
        recursively_destroy_sexp(sx->_body, tag);
    }

    ulong nc = sx->_chld.size();
    qtree* ch = 0;
    for (ulong i =0 ; i < nc; ++i) {
        ch = sx->_chld[i];
        if (ch) {
            recursively_destroy_sexp(ch,tag);
        }
    }
    
    destroy_sexp(sx);
}




// main thing called by repl
//
// if parse is incomplete, then don't 'consume' those characters.
//
//  ownership: the tdop can be killed at the end of this call. Clients should transfer the sealed sexp to the client's ptag,
//   before doing so. pointers to the new sexp are already on the client's stk, so this should be easy.
//
//  tdop are no longer marked as sysbuiltin so that we can track them if they leak.
//
void  parse_sexp(char *s, size_t len, Tag* ptag, parse_cmd& cmd, long* nchar_consumed, l3obj* env, l3obj** retval_tdop, FILE* ifp) {
    
    qqchar a(s,s+len);

    l3obj* obj = (l3obj*)&a;

    l3obj* mytdop = 0;
    tdop(obj,0,0,   env,&mytdop,ptag,  (l3obj*)&cmd,t_tdo,ptag,ifp);

    *retval_tdop = mytdop;

    // we say we are system builtin; this lets us parse command lines without failing allbultin/checkzero checks.
    // no longer needed, tdop should be deleted in queue_up_some_sexpressions():    set_sysbuiltin(mytdop);

    *nchar_consumed = cmd._nchar_consumed;

}


void sexpobj_print(l3obj* obj,const char* indent, stopset* stoppers) {

    LIVEO(obj);
    assert(obj->_type == t_sxp);

    l3path more(0,"%s      ",indent);

    sexpobj_struct* sxs = (sexpobj_struct*)obj->_pdb;
    if (sxs && sxs->_psx) {
        printf("%ssexp, original text:\n\n",indent);
        sxs->_psx->printspan(0,more());

        printf("\n%ssexp, parse:\n",more());

        l3bigstr d;
        sxs->_psx->stringify(&d,more());
        d.outln();
    } 
  
}


void  qtree::min_descendant_b(char** minsofar) {

    if (_val.b < *minsofar) {
        *minsofar = _val.b;
    }

    if (_headnode) {
        _headnode->min_descendant_b(minsofar); 
    }

    if (_body) {
        _body->min_descendant_b(minsofar); 
    }
     
    ulong nc = _chld.size();
    for (ulong i =0 ; i < nc; ++i) {
         _chld[i]->min_descendant_b(minsofar);
    }
}
 
 
 void qtree::max_descendant_e(char** maxsofar) {

     if (_qfac && _qfac->_extendsto[_id] >= 0) {
         long ext = _qfac->_extendsto[_id];

         char* ee = _qfac->_tkend[ext];
         if (ee > *maxsofar) {
             *maxsofar = ee;
         }
     }

     if (_val.e > *maxsofar) {
         *maxsofar = _val.e;
     }

     // catch closing paren if need be:
     if (_bookend.e) {
         if (_bookend.e > *maxsofar) {
             *maxsofar = _bookend.e;
         }
     }

     if (_body) {
         _body->max_descendant_e(maxsofar); 
     }
     
     if (_headnode) { 
         _headnode->max_descendant_e(maxsofar); 
     }
     
     ulong nc = _chld.size();
     for (ulong i =0 ; i < nc; ++i) {
         _chld[i]->max_descendant_e(maxsofar);
     }
 }



// serialization for tdopout_struct

void tdopout_struct::serialize_to(l3path& fname) {

    if (0 == _parsed) return;

        int fd = open(fname(), O_WRONLY);

        l3ts::qtreepb* p = new l3ts::qtreepb;

        _parsed->serialize(p);

        if (!p->SerializeToFileDescriptor(fd)) {
            fprintf(stderr, "Failed to serialize protocol buffer message to file '%s'.\n",fname());
            assert(0);
        }
        close(fd);
}

void tdopout_struct::unserialize_from(l3path& fname) {
    
    int fd = open(fname(), O_RDONLY);
    
    l3ts::qtreepb* reinflated = new l3ts::qtreepb;
    
    if (!reinflated->ParseFromFileDescriptor(fd)) {
        fprintf(stderr, "Failed reinflate test data '%s'.\n",fname());
        delete reinflated;
        l3throw(XABORT_TO_TOPLEVEL);
    }

    close(fd);
    
    DV(std::cout << "reinflated: " << reinflated->DebugString() << std::endl);

    // store into _parsed...?

    //   return reinflated;
}




//
// obj = an l3ts::Any* to read from
//
// env = an l3ts::nameval* to read from
//
L3METHOD(tdo_load)
{

   printf("method: tdo_load called!\n");

    l3ts::nameval* nv = 0;
    if (env) {
        nv = (l3ts::nameval*)env;

    } else if (obj) {
        l3ts::Any* any = (l3ts::Any*) obj;
        assert(any->has__nameval());
        
        nv = any->mutable__nameval();
    }

    //DV
    printf("tdo_load, nv: \n%s\n",nv->DebugString().c_str());

    assert(nv->has_nvtyp());
    l3ts::nameval_Type nvTy = nv->nvtyp();
    if (nvTy != l3ts::nameval_Type_FUN) {
        printf("error: tdo_load called by nameval subtype is not FUN, but rather unexpected enum value: %d\n", nvTy);
        l3throw(XABORT_TO_TOPLEVEL);
    }

    // check first to see if we already have the double vector/matrix in memory by uuid.
    // if so, no need to load it again.
    
    std::string pbsn = nv->pbsn();
    if (pbsn.size()) {
        l3obj* alreadyhere = (l3obj*)serialfactory->uuid_to_ptr(pbsn);
        if (alreadyhere) {
            LIVEO(alreadyhere);
            *retval = alreadyhere;
            return 0;
        }
    }


    // make a new t_nv3 l3obj wrapper object. refer to it with nobj.
    l3path newnm("tdo_load_string_obj");
    l3obj* nobj = make_new_string_obj(0,retown, newnm());

    bool sparse = (nv->idx_size() > 0);

    // sanity check
    if (sparse) {
        long idx_sz = nv->idx_size();
        long tdo_sz = nv->fun_size();
        if (idx_sz != tdo_sz) {
            
            printf("error: tdo_load found sparse string array "
                   "(idx_sz is %ld) however we have "
                   "non-matching tdo_sz of %ld.\n",
                   idx_sz, 
                   tdo_sz
                   );
            l3throw(XABORT_TO_TOPLEVEL);
        }
    }

    long sz = nv->fun_size();
    long idx = 0;
    for (long i = 0; i < sz; ++i) {
        idx = i;
        if (sparse) {
            idx = nv->idx(i);
        }
        string_set(nobj,idx, (char*)nv->str(i).c_str());
    }

    if (nv->has_pbsn()) {
        serialfactory->link_ptr_to_uuid(nobj, nv->pbsn());
    }

    *retval = nobj;
}
L3END(tdo_load)



/// tdo_save : internal method, shouldn't be expected to handle sexp_t input. To be called by a generic (save) method.
//
// obj = what to serialize, an l3obj* of t_str
//
// exp   = l3ts::Any*     <- store into this for saving, if set; unless curfo is set.
// env = l3ts::nameval* <- store into this for saving, takes precedence over exp.
// curfo = stopset, but not used for strings
//
L3METHOD(tdo_save)
{
    assert(obj);
    assert(exp || env);
    assert (obj->_type == t_str);

    l3ts::Any*    any = 0;
    l3ts::nameval* nv = 0;

    if (env) {
        nv = (l3ts::nameval*)env;

    } else if (exp) {
        any = (l3ts::Any*) exp;
        nv = any->mutable__nameval();

    } else {
        assert(0); exit(1);
    }

    nv->set_nvtyp(l3ts::nameval::STR);
    l3path pbsn;
    ptr_to_uuid_pbsn(obj, pbsn);

    nv->set_pbsn(pbsn());

    long  idx = 0;
    char* next = 0;
    bool  sparse = string_is_sparse(obj);

    if (string_first(obj,&next,&idx)) {
        nv->add_str(next);
        if (sparse) { nv->add_idx(idx); }
        
        while(string_next(obj,&next,&idx)) {
            nv->add_str(next);
            if (sparse) { nv->add_idx(idx); }
        }
    }
    
    *retval = gtrue;
}
L3END(tdo_save)


void test_pratt() {

}



//
// set fresh_new if qtree was just newed up and this is assigning ownership.
//
void transfer_subtree_to(qtree* moveme, Tag* new_owner) {
    if (0 == moveme) return;

    LIVET(moveme->_owner);
    LIVET(new_owner);

    // move me
    if (moveme->_owner != new_owner) {
        
        assert(moveme->_owner->sexpstack_exists(moveme));
        moveme->_owner->sexpstack_remove(moveme);
        
        moveme->_owner = new_owner;
        new_owner->sexpstack_push(moveme);

        // and move my mobile contents...
        moveme->_val.transfer_to(new_owner);
        moveme->_span.transfer_to(new_owner);
        moveme->_bookend.transfer_to(new_owner);

    }
    

    // and move my descedants, and so verifying they are owned by new owner
    //  even if I already am.
    if (moveme->_headnode) { transfer_subtree_to(moveme->_headnode, new_owner); }
    if (moveme->_body)     { transfer_subtree_to(moveme->_body, new_owner); }

    long nc = moveme->nchild();
    if (nc) {
        for (long i= 0; i < nc; ++i) {
            transfer_subtree_to(moveme->ith_child(i), new_owner);
        }
    }

}


void recursively_set_sysbuiltin(qtree* exp) {
    assert(exp);
    assert(is_sealed(exp));
    set_sysbuiltin(exp);

     if (exp->_body) {
         recursively_set_sysbuiltin(exp->_body);
     }
     
     if (exp->_headnode) { 
         recursively_set_sysbuiltin(exp->_headnode);
     }
     
     ulong nc = exp->_chld.size();
     for (ulong i =0 ; i < nc; ++i) {
         recursively_set_sysbuiltin(exp->_chld[i]);
     }    
}

void recursively_set_notsysbuiltin(qtree* exp) {
    assert(exp);
    assert(is_sealed(exp));
    set_notsysbuiltin(exp);

     if (exp->_body) {
         recursively_set_notsysbuiltin(exp->_body);
     }
     
     if (exp->_headnode) { 
         recursively_set_notsysbuiltin(exp->_headnode);
     }
     
     ulong nc = exp->_chld.size();
     for (ulong i =0 ; i < nc; ++i) {
         recursively_set_notsysbuiltin(exp->_chld[i]);
     }    
}




#if 0 // not quite ready yet, as a replacement for make_node()

qtree* make_new_qtree(long id, const qqchar& where, t_typ ty, Tag* ptag, qqchar* bookend, int lbp, int rbp) {

    quicktype* qt = 0;
    t_typ known_type = qtypesys->which_type(ty, &qt);
    if (!known_type) {
        printf("error in make_new_qtree: unknown type '%s', could not get _qtreenew for it.\n", ty);
        l3throw(XABORT_TO_TOPLEVEL);
    }
    assert(qt);

    ptr2new_qtree ctor = qt->_qtreenew;

    qtree* s = 0;

    // if we have one, call it.
    if (ctor) {
        s = ctor(where, ty, ptag);
    } else {
        // put in an atom
        s = new_t_ato(where);
    }


    // and set val correctly -- *must* do this to provide _val setting.
    s->set_val(this);

    if (bookend) {
        s->set_bookend(*bookend);
    }

    s->_type = t_qtr;
    s->_owner = _tdo->_myobj->_mytag;

    s->_ser = serialfactory->serialnum_sxp(s, s->_owner);
    s->_owner->sexpstack_push(s);

    s->_reserved = 0;
    //    set_sysbuiltin(s);
    s->_malloc_size = sizeof(qtree);


    if (s->_ty) {
        assert(s->_ty == _tky[id]);
    }
    s->_ty = _tky[id];


    l3path varname(0,"qtree_node_of_%s_at_%p", _tky[id], s);
    l3path msg(0,"777777 %p new qtree node '%s'  @serialnum:%ld\n",s,varname(),s->_ser);
    DV(printf("%s\n",msg()));
    MLOG_ADD(msg());


    // from make_node()


    if (id >= _tksz || id < 0) return 0;
    
    if (lbp < 0) lbp = 0;

    oid2qtree_it it = _symtable.find(id);
    qtree* s = 0;
    
    if (it != _symtable.end()) {
        s = it->second;
        
        if (lbp > s->_lbp) {
            s->_lbp = lbp;
        }
        
    } else {
        // get a pointer to our new_{type} function
        ptr2new_qtree ctor = _tksem[id];

        qqchar vnew(_tk[id], _tkend[id]);

        // if we have one, call it.
        if (ctor) {
            s = ctor(id,vnew);
        } else {
            // probably need to augment the quicktype.cpp table with another ctor.
            assert(0);

            // put in an atom  - not good enough 
            s = new_t_ato(id,vnew);
        }

        s->set_qfac(this);

        long ext = _extendsto[id];
        if (ext > -1) {
            qqchar ender( _tk[ext], _tkend[ext]);            
            s->set_bookend(ender);
        }

        s->_type = t_qtr;
        s->_owner = _tdo->_myobj->_mytag;

        s->_ser = serialfactory->serialnum_sxp(s, s->_owner);
        s->_owner->sexpstack_push(s);

        s->_reserved = 0;

        // hiding leaks:
        //set_sysbuiltin(s);

        s->_malloc_size = sizeof(qtree);


        if (s->_ty) {
            assert(s->_ty == _tky[id]);
        }
        s->_ty = _tky[id];


        l3path varname(0,"qtree_node_of_%s_at_%p", _tky[id], s);
        l3path msg(0,"777777 %p new qtree node '%s'  @serialnum:%ld\n",s,varname(),s->_ser);
        DV(printf("%s\n",msg()));
        MLOG_ADD(msg());


        _symtable.insert(hashmap_oid2qtree::value_type(id, s));

        if (s) {
            if (lbp > s->_lbp) {
                s->_lbp = lbp;
            }
        } 
    }
    return s;

    // the C++ ctors


    qtree(long id, const qqchar& v)
        : 
        _id(id),
        _val(v),
        _lbp(0),
        _rbp(0),
        _ty(0)
        ,_headnode(0)
        ,_closer(0)
        ,_prefix(false)
        ,_postfix(false)
        ,_body(0)
        ,_extendsto(0)
        { 
            munch_right = &base_munch_right;
            munch_left  = &base_munch_left;
        }

    qtree(long id, const qqchar& v, int lbp, int rbp, t_typ ty)
        : _id(id),
        _val(v),
        _lbp(lbp),
        _rbp(rbp),
        _ty(ty)
        ,_headnode(0)
        ,_closer(0)
        ,_prefix(false)
        ,_postfix(false)
        ,_body(0)
        ,_extendsto(0)
        {  
            munch_right = &base_munch_right;
            munch_left  = &base_munch_left;
        }



}


#endif


