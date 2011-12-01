//
// Copyright (C) 2011 Jason E. Aten. All rights reserved.
//
#ifndef LEX_TWOPOINTER_H
#define LEX_TWOPOINTER_H


#include "quicktype.h"
#include <vector>

struct qtreefactory;
struct _l3obj;

// quick s-expression node

class codebase {
 public:

    // the start of the code
    char*         _base;

    // char* are direct pointers, >= _base.
    std::vector<char*> _tk;
    std::vector<char*> _tkend;
    std::vector<t_typ> _tky;
    std::vector<ptr2new_qtree> _tksem;

};

class qexp {
 public:
    MIXIN_MINHEADER()

    qexp();
    ~qexp();
    qexp(const qexp& src);
    qexp& operator=(const qexp& src);

        private:
    void CopyFrom(const qexp& src);

    t_typ  _qxty; // *not* t_qxp, but rather the type of the node/token, e.g. t_opn for '(' token.

    // pointers to the begin and end+1 of the text 
    char*  _val;
    char*  _valstop;

    // to get the serialization of me
    // and all my children, print from _val to _spanstop.
    char*  _spanstop; 
    
    std::vector<qexp*> _ch; // children
    
};


long addlexed(const char* start, const char* stop, char** d /*destination*/, bool chomp = true);

void lex(char* start, long sz, std::vector<char*>& tk, std::vector<char*>& tkend, std::vector<t_typ>& tky, std::vector<ptr2new_qtree>& tksem, const char* filename);

void dumpta(
            std::vector<char*>& tk, 
            std::vector<char*>& tkend, 
            std::vector<t_typ>& tky,
            std::vector<long>* ext = 0,
            std::vector<bool>* ignore = 0,
            qtreefactory* qfac = 0
);



void gen_protobuf(
                              char* filename,
                              char* start,
                              char* protostart,
                              long  sz,
                              std::vector<char*>& tk, 
                              std::vector<char*>& tkend, 
                              std::vector<t_typ>& tky,
                              std::vector<ptr2new_qtree>& tksem,
                              std::vector<long>& extendsto,
                              std::vector<bool>& ignore
                  );

void parsel3(
                              char* filename,
                              char* start,
                              char* protostart,
                              long  sz,
                              std::vector<char*>& tk, 
                              std::vector<char*>& tkend, 
                              std::vector<t_typ>& tky, 
                              std::vector<ptr2new_qtree>& tksem,
                              std::vector<long>& extendsto,
                              std::vector<bool>& ignore,
                              _l3obj** retval,
                              Tag*    retown
                  );

void compute_extends_to(
                              const char* filename,
                              const char* start,
                              const char* protostart,
                              long  sz,
                              std::vector<char*>& tk, 
                              std::vector<char*>& tkend, 
                              std::vector<t_typ>& tky,
                              std::vector<ptr2new_qtree>& tksem,
                              std::vector<long>& extendsto,
                              std::vector<bool>& ignore,
                              bool  partialokay  // set to true to allow partial parses/unmatched parentheses.
                  );


void ignore_cpp_comments(
                              const char* filename,
                              const char* start,
                              const char* protostart,
                              long  sz,
                              std::vector<char*>& tk, 
                              std::vector<char*>& tkend, 
                              std::vector<t_typ>& tky,
                              std::vector<ptr2new_qtree>& tksem,
                              std::vector<long>& extendsto,
                              std::vector<bool>& ignore,
                              bool  partialokay  // set to true to allow partial parses/unmatched parentheses.
                  );


void dump(std::vector<char*>& tk, std::vector<char*>& tkend, std::vector<t_typ>& tky);


void match_braces(
                  const char* filename,
                  char* start,
                  char* protostart,
                  long  sz,
                  std::vector<char*>& tk, 
                  std::vector<char*>& tkend, 
                  std::vector<t_typ>& tky, 
                  std::vector<ptr2new_qtree>& tksem,
                  std::vector<long>& extendsto,
                  std::vector<bool>& ignore,
                  bool partialokay
                  );


void match_square_brackets(
                  const char* filename,
                  char* start,
                  char* protostart,
                  long  sz,
                  std::vector<char*>& tk, 
                  std::vector<char*>& tkend, 
                  std::vector<t_typ>& tky, 
                  std::vector<ptr2new_qtree>& tksem,
                  std::vector<long>& extendsto,
                  std::vector<bool>& ignore,
                  bool partialokay
                           );


void match_triple_quotes(
                  const char* filename,
                  char* start,
                  char* protostart,
                  long  sz,
                  std::vector<char*>& tk, 
                  std::vector<char*>& tkend, 
                  std::vector<t_typ>& tky, 
                  std::vector<ptr2new_qtree>& tksem,
                  std::vector<long>& extendsto,
                  std::vector<bool>& ignore,
                  bool partialokay
                           );



void mark_within_single_quotes_as_ignored(
                  const char* filename,
                  char* start,
                  char* protostart,
                  long  sz,
                  std::vector<char*>& tk, 
                  std::vector<char*>& tkend, 
                  std::vector<t_typ>& tky, 
                  std::vector<ptr2new_qtree>& tksem,
                  std::vector<long>& extendsto,
                  std::vector<bool>& ignore,
                  bool partialokay
                           );


void dumplisp(
              char* start,
              char* protostart,
              std::vector<char*>& tk, 
              std::vector<char*>& tkend, 
              std::vector<t_typ>& tky, 
              std::vector<long>& ext,
              std::vector<bool>& ignore);


#endif /* LEX_TWOPOINTER_H */
