 //
 // Copyright (C) 2011 Jason E. Aten. All rights reserved.
 //
 #ifndef L3PRATT_H
 #define L3PRATT_H

 #include "qexp.h"
 #include "l3obj.h"
 #include "terp.h"

 #include "quicktype.h"
 #include <vector>
 #include <string>
 #include "merlin.pb.h"
 #include "lex_twopointer.h" // for addlexed()
 #include <list>
 #include "l3ts.pb.h"

 using std::vector;


 struct qtree;
 struct tdopout_struct;

 typedef std::tr1::unordered_map<long, qtree*>    hashmap_oid2qtree;
 typedef hashmap_oid2qtree::iterator               oid2qtree_it;


  // the tdop takes this as an arg, in curfo position.
struct parse_cmd {
      MIXIN_TYPSER(); // so we can verify type.

      // optional place to store resulting qtree*. if 0, not stored there, obviously.
      ustaq<sexp_t>* _rstk;

      // for calling get_more_input(), which is on by default.
      bool    _forbid_calling_get_more_input;
      FILE*   _fp;
      l3path* _prompt;
      bool    _show_prompt_if_not_stdin;
      bool    _echo_line;
      const char*   _filename;
    
      // policy for this call
      bool _partial_queueit; 
      bool _partial_throw;   

      // results of this call:
      long _nchar_consumed;
      bool _was_incomplete;

      parse_cmd() { 
          init(); 
      }

      void init() {
          bzero(this,sizeof(parse_cmd));
          _type = t_pcd;
      }
};


 long tk_lineno(long errtok, vector<t_typ>& _tky);

 void tk_print_error_context(long errtok, 
                             const char* filename,
                             vector<char*>& _tk, 
                             vector<char*>& _tkend, 
                             vector<t_typ>& _tky
                             );

 // a factory for qtree* nodes
 class qtreefactory {
  public:
     qtreefactory(
               vector<char*>& tk, 
               vector<char*>& tkend, 
               vector<t_typ>& tky,
               vector<ptr2new_qtree>& tksem,
               vector<long>& extendsto,
               vector<bool>& ignore,
               tdopout_struct* tdo
               );

     ~qtreefactory();

     qtree*  make_node(long id, int lbp);

     // returns type of token, in case we want to check.
     //
     // expected, if supplied, usually means that this is what we are expecting and we just want to ignore that token... like ')', or ']' or '}'
     //
     t_typ advance(t_typ expected);


     //
     // reset to start a parse completely over:
     //
     void reset() {

         _next = 0;
         _tksz = _tk.size();
         _accumulated_parsed_qtree = 0;
         _token = 0;
         _cur = 0;
         _eof = false;
         clear_symtable();
         _cnode_stack.clear();
     }

     void prep_to_resume() {
         _tksz = _tk.size();
         _eof = false;
         //         _next--;
         advance(0);
     }

     void clear_symtable();


     // expression():
     //
     // From Douglas Crockford's article on Pratt parsing: "Top Down Operator Precedence"
     //       http://javascript.crockford.com/tdop/tdop.html
     //
     //    The heart of Pratt's technique is the expression function. It takes 
     // a right binding power that controls how aggressively it binds to tokens on its right.
     // expression calls the nud method of the token. The nud is used to process 
     // literals, variables, and prefix operators. Then as long as the right binding 
     // power is less than the left binding power of the next token, the led method 
     // is invoked on the following token. The led is used to process infix and 
     // suffix operators. This process can be recursive because the nud and led 
     // methods can call expression.
     //
     // In pseudo Java script:
     //
     // var expression = function (rbp) {
     //    var left;
     //    var t = token;
     //    advance();
     //    left = t.nud();
     //    while (rbp < token.lbp) {
     //        t = token;
     //        advance();
     //        left = t.led(left);
     //    }
     //    return left;
     // }
     //
     // our expression:
     //
     qtree* expression(int rbp);

     void    symbol_print(qtree* s, const char* indent);
     qtree*  node_exists(long id);
     void    delete_id(long id);

     //
     // copy a span of tokens into the buffer starting at s. Return number of char written.
     //
     long copyto(char* s, ulong cap, const tokenspan& ts) {
         assert(ts._e >= ts._b);
         if (cap == 0) return 0;

         char* p = s;
         char* stop = s + cap;

         // note the <= : because _e is *inclusive*, *not exclusive*.
         for (long i = ts._b; i <= ts._e; ++i) {
             for (char* j = _tk[i]; j < _tkend[i]; ++j) {
                 *p++ = *j;
                 if (p == stop) return (p-s);
             }
             // put a space after each token... for readability and for the '\0' delimiter.
             *p++ = ' ';
             if (p == stop) return (p-s);
         }

         *(p-1) = 0; // end with terminator, not space.
         return (p-s);
     }

     long request_more_input(parse_cmd& cmd);

     qtree* test_postfix_parsing(int bp);

     bool eof() { return _eof || _next >= _tksz; }

     // if errtok is unexpected, print the line it is on with
     // an ^ indicator underneath.
     //
     void print_error_context(long errtok);

     inline qtree* token() { return _token; }
     inline long   next() { return _next; }
     inline long   curidx() { return _cur; }

     void dump_symtable();

     hashmap_oid2qtree _symtable;

     vector<char*>& _tk;
     vector<char*>& _tkend;
     vector<t_typ>& _tky;
     vector<ptr2new_qtree>& _tksem;
     vector<long>&   _extendsto;
     vector<bool>&   _ignore;

     // 
     void as_checkable_string(l3ts::qtreepb*, l3path* d);
     void as_short_string(l3ts::qtreepb*, l3bigstr* d, const char* ind);

     ulong cstack_size() { return _cnode_stack.size(); }

     ulong last_index() { return _tksz-1; }


     tdopout_struct*            _tdo;

     void set_parse_cmd(const parse_cmd& cmd) { _parcmd = cmd; }

  private:
     parse_cmd _parcmd;

     long _tksz;
     long _next;

     qtree* _accumulated_parsed_qtree;

     qtree* _token;
     long   _cur; // the index that corresponds to _token. (_next -1)

     bool   _eof; // can come early with just newlines at the end


     // to make for pausable and resumable parsing, we want to make the
     // explicity the cnode expression stack, which would normally hold t / cnode.
     // Recall that 't' in Pratt's paper became cnode (current node) in my renaming for better
     // documentation effort.
     std::list<qtree*> _cnode_stack;

     bool _incomplete;

 };




 // this direct call was used during development, but is
 //  now mostly unused, being displaced by the tdop/addtxt object interface.
 void pratt(
                               char* filename,
                               char* start,
                               char* lispstart,
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
                  );




 struct qtree;

 typedef qtree* (*ptr2munch_right) (qtree* _this_);
 typedef qtree* (*ptr2munch_left)  (qtree* _this_,  qtree* left);


 //ptr2munch_right  base_munch_right;
 //ptr2munch_left   base_munch_left;

 qtree* base_munch_right(qtree* _this_);
 qtree* base_munch_left(qtree* _this_, qtree* left);


 //
 // qtree: root of class hierarchy (that isn't really a hierarchy anymore).
 //

 // Very Important NO VTABLE note: subclasses cannot add their own members, since they
 //  will all be deleted via a qtree*.  Any additional data members must
 //  go into qtree itself.  This is so we can get rid of the stupid vtable,
 //  and thus track the memory accurately using the Tag/_ser number system.
 //
 //  subclasses are really just to make it easier to initialize properly.
 //
 //  we do use new/delete so as to take of the vector properly, but this
 //   may changle to placement new + explicity dtor  with malloc/free in
 //   the future.
 //

 struct qtree {
     MIXIN_MINHEADER()
     // gives us:
     //     t_typ      _type;  // always t_qtr for qtree*
     //     long       _ser;   // allocated by serialfactory->serialnum_sxp()
     //     uint       _reserved;
     //     uint       _malloc_size;
     //     Tag*       _owner;

  public:

     //////////////////////////////
     //
     // public api to qtree:
     //

     qtree(long id, const qqchar& v)
     : 
     _id(id)
         ,_val(v)
         ,_lbp(0)
         ,_rbp(0)
         ,_ty(0)
         ,_headnode(0)
         ,_closer(0)
         ,_prefix(false)
         ,_postfix(false)
         ,_body(0)
         ,_qfac(0)
         ,_myobj(0)
     { 
         munch_right = &base_munch_right;
         munch_left  = &base_munch_left;
     }

      qtree(long id, const qqchar& v, int lbp, int rbp, t_typ ty)
      : 
          _id(id)
          ,_val(v)
          ,_lbp(lbp)
          ,_rbp(rbp)
          ,_ty(ty)
          ,_headnode(0)
          ,_closer(0)
          ,_prefix(false)
          ,_postfix(false)
          ,_body(0)
          ,_qfac(0)
          ,_myobj(0)
    {
        munch_right = &base_munch_right;
        munch_left  = &base_munch_left;
    }

    qtree(const qtree& src) {
        CopyFrom(src);
    }

    qtree& operator=(const qtree& src) {
        if (&src != this) {
            CopyFrom(src);
        }
        return *this;
    }

    void CopyFrom(const qtree& src) {

        _type = t_qtr;
        _ser = serialfactory->serialnum_sxp(this, src._owner);
        _reserved    = src._reserved;
        _malloc_size = src._malloc_size;
        _owner       = src._owner;
        _owner->sexpstack_push(this);

        _id  = src._id;
        _val = src._val;
        _lbp = src._lbp;
        _rbp = src._rbp;
        _ty  = src._ty;

        l3path varname(0,"qtree_node_of_%s_at_%p_CopyFrom_%p", _ty, this,&src);
        l3path msg(0,"777777 %p new qtree node '%s'  @serialnum:%ld\n",this,varname(),_ser);
        DV(printf("%s\n",msg()));
        MLOG_ADD(msg());

        munch_right = src.munch_right;
        munch_left  = src.munch_left;

        _closer = src._closer;
        _prefix  = src._prefix;
        _postfix = src._postfix;

        _qfac    = src._qfac;
        _bookend = src._bookend;
        _span    = src._span;

        _stringification_compressed = src._stringification_compressed;
        _stringification_viewable = src._stringification_viewable;


        if (src._headnode) {
            _headnode = new qtree(*(src._headnode));
        } else {
            _headnode = 0;
        }
        if (src._body) {
            _body     = new qtree(*(src._body));
        } else {
            _body = 0;
        }

        long nc = src._chld.size();
        if (nc) {
            for (long i =0; i < nc; ++i) {
                if (src._chld[i]) {
                    _chld.push_back( new qtree(*src._chld[i]));
                } else {
                    assert(0); // really we don't want empty child nodes.
                    _chld.push_back(0);
                }
            }
        }

        _myobj = src._myobj;
    }


    //   "The nud is used to process literals, variables, and prefix operators."
    //   i.e. "Munch tokens to the right in the input stream"
    //   so we rename 'nud' to 'munch_right'

    ptr2munch_right munch_right;

     //  "The led is used to process infix and suffix operators."
     //  i.e. "Munch tokens from the left in the input stream"
     //  so we rename 'led' to 'munch_left'

    ptr2munch_left munch_left;

    int arity() { return _chld.size(); }


    // print()
    //
    // PRE: _val is set. i.e. set_val already called.
    //
    void print(const char* indent = "") {
        assert(_val.b);
        assert(_val.e);

        l3path im(0,"%s      ",indent);
        l3path im2(0,"%s      ",im());


         printf("%ssymbol %p is:\n",indent,this);
         
         l3path begend;
         begend.p += addlexed(_val.b, _val.e, &(begend.p));
         
         printf("%s    '%s' : '%s'\n", indent, _ty, begend());
         printf("%s      _lbp:%d   _rbp:%d\n", indent, _lbp, _rbp);


         if (_headnode) {
             printf("%s_headnode of %p\n",indent,this);
             _headnode->print(im2());
         }

         if (_prefix) {
             printf("%s_prefix flag set on %p\n",indent,this);
         }

         if (_postfix) {
             printf("%s_postfix flag set on %p\n",indent,this);
         }

         if (_body) {
             printf("%s_body of %p\n",indent,this);
             _body->print(im2());
         }

         
         for (ulong i = 0; i < _chld.size(); ++i) {
             printf("%schild %ld :\n",indent,i);
             
             _chld[i]->print(im());
         }
         
         if (_chld.size()==0) {
             printf("%s<no children>.\n",indent);
         }
         
         
    } //end print()
 

    
    // for ease of gdb access to printing nodes.
    void p() {
        print_stringification(0);
    }

    void val(l3path *addto) {
        assert(addto);
        addto->p += addlexed(_val.b, _val.e, &(addto->p));
    }

    void val(l3bigstr *addto) {
        assert(addto);
        addto->p += addlexed(_val.b, _val.e, &(addto->p));            
    }

    qqchar val() {
        assert(_val.b);
        assert(_val.e);
        return _val;
    }

    qqchar headval() {
        qqchar r;
        if (_headnode) {
            r = _headnode->val();
        }
        return r;
    }

 
    //
    // get a qqchar that covers all the desecendants of this node.
    //
    qqchar span() const {
        return _span;
    }

    void span(qqchar& put_span_here) const {

        put_span_here = _span;
    }

    void compute_span() {
        if (is_sealed(this)) return;

        assert(0 == _span._owner);

        _span = _val;
        min_descendant_b( &_span.b);
        max_descendant_e( &_span.e);
    }

    void min_descendant_b(char** min);
    void max_descendant_e(char** max);


    void print_stringification(int fd = 0) {
        l3bigstr big;
        stringify(&big,"");
        write(fd,big(),big.len());
        write(fd,"\n",1);
        ::fdatasync(fd);
    }

     void printspan(int fd, const char* indent) {
         ::write(fd, indent, strlen(indent));
         _span.write_with_indent(fd, indent);
         write(fd,"\n",1);
         //         ::fdatasync(fd);
    }


    qtree* headnode() {
        if (_headnode == 0) return 0;
        return _headnode;
    }

    qtree* first_child() {
        if (_chld.size() == 0) return 0;
        return _chld[0];
    }

    qtree* ith_child(long i) {
        assert(i >= 0);
        if (_chld.size() == 0) return 0;
        return _chld[i];
    }

 

    // last thing at liftoff (after parsing is complete): 
    //   Seal it so that it becomes independet of its originating qtreefactory.
    //   This also tells it to get _extendsto set. No more changes after this.
    //
     void seal() {
         assert(_val.b);
         compute_span();


         // go independent. Tests whether we can reinflate from disk without pointing to Permatext.
         LIVET(_owner);
         _val.ownit(_owner);
         _span.ownit(_owner);
         _bookend.ownit(_owner);



         set_sealed(this);
     
         // and recurse

         if (_body) {
             _body->seal();
         }

         if (_headnode) {
             _headnode->seal();
         }
         
         for (ulong i = 0; i < _chld.size(); ++i) {
             _chld[i]->seal();
         }
     }


     l3ts::qtreepb* serialize(l3ts::qtreepb* myser) {

         // do the basic serialization
         assert(myser);

         //        myser->set__id(_id);
         myser->set__lbp(_lbp);
         myser->set__rbp(_rbp);
         myser->set__ty(_ty);

         l3path begend;
         begend.p += addlexed(_val.b, _val.e, &(begend.p));

         myser->set__val(begend());

         for (ulong i = 0; i < _chld.size(); ++i) {
             _chld[i]->serialize( myser->add__chld() );
         }

         // add our array reference
         if (_headnode) {
             l3ts::qtreepb*  hd = myser->mutable__head();
             _headnode->serialize(hd);
         }

         // body
         if (_body) {
             l3ts::qtreepb*  bd = myser->mutable__body();
             _body->serialize(bd);
         }

         if (_span.len()>0) {

             l3bigstr bigspan;
             bigspan.p += addlexed(_span.b, _span.e, &(bigspan.p));
             myser->set__span(bigspan());
         }

         if (_closer) {
             myser->set__closer(_closer);
         }


         return myser;
     }


    void stringify(l3bigstr* d, const char* ind) {
        assert(d);

        l3path inmore;

        if (ind) {
            d->pushf("\n%s",ind);
            inmore.reinit("%s      ",ind);
        } else {
            d->pushf("(");
        }

        l3path val;
        val.p += addlexed(_val.b, _val.e, &(val.p));

        d->pushf("'%s'",val());

        //now we've inlined: stringify_prechildren(d,ind) :
        l3path im(0,"%s        ",(char*)ind);
        l3path nim(0,"\n%s",im());

        if (_headnode) {
            if (ind) { d->pushf("%s<%s:>", nim(), "_headnode"); } else { d->pushf(" %s:","_hd");}
            _headnode->stringify(d, ind ? im() : 0);
            if (ind) { d->pushf("%s</%s>", nim(), "_headnode"); }
        }
         
        //
        //  If we have _postfix/_prefix, the actual arguments are stored in _chld now,
        //  but we still print them as special, to make it easy to confirm that parsing
        //  was correct.
        //

        if (_postfix) {
            if (ind) { d->pushf("%s<postfix:>", nim()); } else { d->pushf(" postfix:");}
            assert(_chld.size() == 1);
            _chld[0]->stringify(d, ind ? im() : 0);
            if (ind) { d->pushf("%s</postfix>", nim()); }
        }
        
        if (_prefix) {
            if (ind) { d->pushf("%s<prefix:>", nim()); } else { d->pushf(" prefix:");}
            assert(_chld.size() == 1);
            _chld[0]->stringify(d, ind ? im() : 0);
            if (ind) { d->pushf("%s</prefix>", nim()); } 
        }

        if (_body) {
            if (ind) { d->pushf("%s<body:>", nim()); } else { d->pushf(" body:");}
            _body->stringify(d, ind ? im() : 0);
            if (ind) { d->pushf("%s</body>", nim()); } 
        }

        int nc = _chld.size();

        l3path newmo(0,"\n%s",inmore());

        if (!_postfix && !_prefix) {
            for (int i = 0; i < nc; ++i) {
                if (ind) { d->pushf("%s<chld %d:>", newmo(), i); }
                _chld[i]->stringify(d,ind ? inmore() : 0);
                if (ind) { d->pushf("%s</chld %d>", newmo(), i); }
            }
        }

        if (!ind) {
            d->pushf(")");
        }

        // and cache our results
        if (ind) {
            _stringification_viewable = (*d)();
        } else {
            _stringification_compressed = (*d)();
        }

    } // end stringify()


    void  sexp_string(l3bigstr* d, const char* ind);
   
    // interface to children, in case we change the implementation later.
    inline qtree*     child(unsigned long index) const { 
        if (index >= _chld.size()) return 0;
        return _chld[index];
    }

    inline long       nchild() const {
        return _chld.size();
    }

    inline void      add_child(qtree* addme) {
        _chld.push_back(addme);
    }

    void set_bookend(const qqchar& be) {
        _bookend = be;
    }

    ///////////////////////////////////////////////////////
    //
    //
    // protected, with a psychological force field... not the compiler,
    //  in a big performance-oriented *hack*.
    //

      std::vector<qtree*> _chld;

      long   _id;  // index into parent qtreefactory _tk[] arrays, for error location reporting.
      qqchar _val; // the text that defines us.
      int    _lbp;
      int    _rbp;

      t_typ  _ty;
      qtree* _headnode;

      t_typ  _closer; // the closing token, e.g. t_cpn, t_cbr, t_csq

      // only one of these can ever be true:
      bool _prefix;  // for ++,--
      bool _postfix; // for ++,--

      // c_for_loops:
      // _chld will have the init,test,advance stuff
      // _body has the body statement
      qtree* _body;    // for loops
      qtreefactory* _qfac;

      void set_qfac(qtreefactory* qfac) { _qfac = qfac; }

      qqchar _span; // and the whole span, kids included.

      // location of the matching close paren, close brace, or close square bracket: _bookend.
      qqchar _bookend; // set by set_bookend()

      std::string  _stringification_compressed;
      std::string  _stringification_viewable;

     // all nodes in a qtree can have an l3obj that is 1-1 paired with the qtree, for extension/annotation/notes.
     // if set, _myobj->_sexp should be == this;
     l3obj*   _myobj;


      friend qtree* qtreefactory::expression(int rbp);
      //      friend t_typ qtreefactory::advance(t_typ expected);
      friend void qtreefactory::symbol_print(qtree* s, const char* indent);
      friend qtree*  qtreefactory::make_node(long id, int lbp);
};


#define EXTERN_NEW_QTREE_TYPE(type)  extern qtree*  new_##type(long id, const qqchar& v);

//define CTOR_QTREE_TYPE(type) qtree_##type(const qqchar& v)


// universal constructor... calls the right new method.
qtree* make_new_qtree(long id, const qqchar& where, t_typ ty, Tag* ptag);



void transfer_subtree_to(qtree* move_me_and_descendants, Tag* new_owner);


qtree* infix_munch_left(qtree* _this_, qtree* left);

struct qtree_infix : public qtree {

    //can we just use the base ctor? no. sadly not.
    qtree_infix(long id, const qqchar& v) : qtree(id,v) {
        munch_left  = &infix_munch_left;
        munch_right = &base_munch_right;

    }
    
    qtree_infix(long id, const qqchar& v, int lbp, t_typ ty) 
        : qtree(id,v, lbp, 0, ty)
    { 
        munch_left = &infix_munch_left;        
        munch_right = &base_munch_right;
    }

};

 // short circuiting infix operator, right-associative, like && and ||

qtree* infix_shortckt_munch_left(qtree* _this_, qtree* left);


struct qtree_infix_shortckt : public qtree {

      //can we just use the base ctor? no. sadly not.
      qtree_infix_shortckt(long id, const qqchar& v) : qtree(id,v) {
          munch_left = &infix_shortckt_munch_left;
          munch_right = &base_munch_right;
      }

      qtree_infix_shortckt(long id, const qqchar& v, int lbp, t_typ ty) 
          : qtree(id,v, lbp, 0, ty)
          {
              munch_left = &infix_shortckt_munch_left;
              munch_right = &base_munch_right;
          }
  };

#define DECLARE_INFIX_SHORTCKT(type,prec) struct qtree_##type : public qtree_infix_shortckt { qtree_##type(long id, const qqchar& v) : qtree_infix_shortckt(id,v,prec,type) {} };  EXTERN_NEW_QTREE_TYPE(type);

 DECLARE_INFIX_SHORTCKT(m_lan, 30); //infixr("&&", 30);
 DECLARE_INFIX_SHORTCKT(m_lor, 30); //infixr("||", 30);


qtree* prefix_munch_right(qtree* _this_);

  struct qtree_prefix : public qtree {

      //can we just use the base ctor? no. sadly not.
      qtree_prefix(long id, const qqchar& v) : qtree(id,v) { 
          munch_right = &prefix_munch_right;
          munch_left  = &base_munch_left;
      }

      qtree_prefix(long id, const qqchar& v, int rbp, t_typ ty) 
          : qtree(id,v, 0, rbp, ty)
          { 
              munch_right = &prefix_munch_right;
              munch_left  = &base_munch_left;
          }

  };


 #define DECLARE_PREFIX_QTREE(type,prec) struct qtree_##type : public qtree_prefix      {   qtree_##type(long id, const qqchar& v) : qtree_prefix(id,v,prec,type) {} };  EXTERN_NEW_QTREE_TYPE(type)

 DECLARE_PREFIX_QTREE(m_bng,70);
 DECLARE_PREFIX_QTREE(m_tld,70);


qtree* group_munch_right(qtree* _this_);

qtree* group_munch_left(qtree* _this_, qtree* left);


//
//  common class for sharing the group parsing code: which we just end up copying and pasting...
//
struct qtree_group : public qtree {

    qtree_group(long id, const qqchar& v) : qtree(id,v, 95, 100, t_opn) {
        _closer = t_cpn;
        munch_right = &group_munch_right;
        munch_left  = &group_munch_left;
    }

    qtree_group(long id, const qqchar& v, int lbp, int rbp, t_typ ty, t_typ closer) : qtree(id,v, lbp, rbp, ty) {
        _closer = closer;
        munch_right = &group_munch_right;
        munch_left  = &group_munch_left;
    }

};



qtree* group_with_headnode_munch_left(qtree* _this_, qtree* left);

qtree* t_opn_munch_left(qtree* _this_, qtree* left);



 //
 //   (  t_opn
 //
 struct qtree_t_opn : public qtree_group {

     qtree_t_opn(long id, const qqchar& v) : qtree_group(id,v, 95, 100, t_opn, t_cpn) { 
         munch_left  = &t_opn_munch_left;
         munch_right = &group_munch_right;
     }

 };

EXTERN_NEW_QTREE_TYPE(t_opn)


qtree* t_obr_munch_left(qtree* _this_, qtree* left);


 //
 // {   : t_obr
 //
 struct qtree_t_obr : public qtree_group { 

      qtree_t_obr(long id, const qqchar& v) : qtree_group(id,v, 80, 0, t_obr, t_cbr) {
          munch_left  = &t_obr_munch_left;
          munch_right = &group_munch_right;
      } 

 };
EXTERN_NEW_QTREE_TYPE(t_obr);



qtree* t_lsp_munch_right(qtree* _this_);


//
// qtree_t_lsp should have 0 lbp so that it doesn't suck up any other tokens to it's left.
//
 // ($    t_lsp
 struct qtree_t_lsp : public qtree_group {

     qtree_t_lsp(long id, const qqchar& v) : qtree_group(id,v, 0, 100, t_lsp, t_cpn) { 
         munch_right  = &t_lsp_munch_right;
         munch_left   = &group_munch_left;
     }

 };
EXTERN_NEW_QTREE_TYPE(t_lsp);



 // (:  t_lsc
 struct qtree_t_lsc : public qtree_group {

     qtree_t_lsc(long id, const qqchar& v) : qtree_group(id,v, 0, 100, t_lsc, t_cpn) {
         munch_right  = &t_lsp_munch_right;
         munch_left   = &group_munch_left;
     }
 };
EXTERN_NEW_QTREE_TYPE(t_lsc);


 // """  t_q3q triple quotes

qtree* t_q3q_munch_right(qtree* _this_);
struct qtree_t_q3q : public qtree_group {

     qtree_t_q3q(long id, const qqchar& v) : qtree_group(id,v, 0, 110, t_q3q, t_q3q) {
         munch_left   = &base_munch_left;
         munch_right  = &t_q3q_munch_right;
     }
 };
EXTERN_NEW_QTREE_TYPE(t_q3q);



// '  t_sqo single quotes

qtree* t_sqo_munch_right(qtree* _this_);
struct qtree_t_sqo : public qtree_group {

     qtree_t_sqo(long id, const qqchar& v) : qtree_group(id,v, 0, 110, t_sqo, t_sqo) {
         munch_left   = &base_munch_left;
         munch_right  = &t_q3q_munch_right;
     }
 };
EXTERN_NEW_QTREE_TYPE(t_sqo);





 struct qtree_t_cpn : public qtree {
     qtree_t_cpn(long id, const qqchar& v) : qtree(id,v, 0,0, t_cpn) { }
 };
EXTERN_NEW_QTREE_TYPE(t_cpn);


qtree* postfix_munch_left(qtree* _this_, qtree* left);

struct qtree_postfix : public qtree {

    qtree_postfix(long id, const qqchar& v) : qtree(id,v) { 
        munch_left = &postfix_munch_left;
    }
    
    qtree_postfix(long id, const qqchar& v, int lbp, t_typ ty) 
        : qtree(id,v, lbp, 0, ty)
    { 
        munch_left = &postfix_munch_left;
    }

};


qtree* t_cmd_munch_left(qtree* _this_, qtree* left);


// command/function reference: 
//
//  wrap <expr_stream> in a lisp paren, so that it is parsed like "($ <expr_stream> )".
//
//  i.e. "save obj filename"  ->  ($save obj filename), hence 'save' will be the headnode
//    like in a function call.

struct qtree_t_cmd : public qtree_group {

    // notice that lbp 95 < rbp 100, so this is left-associative: gobbling left-to-right upon ties in precedence level.
    //
     qtree_t_cmd(long id, const qqchar& v) : qtree_group(id,v, 95, 100, t_cmd, t_cpn) { 
         munch_left  = &t_cmd_munch_left;
         munch_right = &group_munch_right;
     }
};
EXTERN_NEW_QTREE_TYPE(t_cmd);

qtree* assign_munch_left(qtree* _this_, qtree* left);

 struct qtree_assign : public qtree {

      qtree_assign(long id, const qqchar& v) : qtree(id,v) {
          munch_left = &assign_munch_left;
      }

      qtree_assign(long id, const qqchar& v, int lbp, t_typ ty) 
          // use rbp = lbp-1 to get a right-associative assignment; e.g. "a = b = 10" => "a = (b = 10)".
          : qtree(id,v, lbp, lbp-1, ty)
          { 
              munch_left = &assign_munch_left;
          }
 };

#define DECLARE_ASSIGN_QTREE(type,prec) struct qtree_##type : public qtree_assign      { qtree_##type(long id, const qqchar& v) : qtree_assign(id,v,prec,type) {} };  EXTERN_NEW_QTREE_TYPE(type);

 DECLARE_ASSIGN_QTREE(t_asi,10); // =

 DECLARE_ASSIGN_QTREE(m_peq,10); // +=
 DECLARE_ASSIGN_QTREE(m_meq,10); // -=

 DECLARE_ASSIGN_QTREE(m_dve,10); // /=
 DECLARE_ASSIGN_QTREE(m_mte,10); // *=
 DECLARE_ASSIGN_QTREE(m_cre,10); // ^=



 EXTERN_NEW_QTREE_TYPE(t_cpn);

EXTERN_NEW_QTREE_TYPE(qtree_base);
EXTERN_NEW_QTREE_TYPE(qtree_infix);
EXTERN_NEW_QTREE_TYPE(qtree_prefix);
EXTERN_NEW_QTREE_TYPE(qtree_postfix);

// extern qtree*  new_qtree_base(long id, const qqchar& v);
// extern qtree*  new_qtree_infix(long id, const qqchar& v);
// extern qtree*  new_qtree_prefix(long id, const qqchar& v);
// extern qtree*  new_qtree_postfix(long id, const qqchar& v);


// define DECLARE_INFIX_QTREE(type,prec) struct qtree_##type : public qtree_infix      {  qtree_##type* clone() const  { return new qtree_##type(*this); } qtree_##type(long id, const qqchar& v) : qtree_infix(id,v,prec,type) {} };  extern qtree*  new_##type(long id, const qqchar& v);

#define DECLARE_INFIX_QTREE(type,prec) struct qtree_##type : public qtree_infix      {   qtree_##type(long id, const qqchar& v) : qtree_infix(id,v,prec,type) {} };  EXTERN_NEW_QTREE_TYPE(type)


 //
 // t_ato have rbp because a[1] is an array reference, and a needs to check for it.
 //
 // they have a lbp of 0 because they should terminate / disallow further munch_left.
 //
 struct qtree_t_ato : public qtree      {

     // precedence of atoms: 110, so that they can be below anything else in the tree, including parenthesis.
     qtree_t_ato(long id, const qqchar& v) : qtree(id,v,0,110,t_ato) {} 

 };
EXTERN_NEW_QTREE_TYPE(t_ato);



//
// things like strings and numbers can be anywhere, as low as you want, in the parse tree.
//  and they just parse to themselves, and so have the trivial munch left/right routines.
//   and they have zero/zero lbp/rbp to stop parsing once they are hit. i.e. they will be leaves.
//
struct qtree_zerozero : public qtree      {

     qtree_zerozero(long id, const qqchar& v) : qtree(id,v,0,0,0) {} 

     // precedence of atoms: 110, so that they can be below anything else in the tree, including parenthesis.
     qtree_zerozero(long id, const qqchar& v, t_typ ty) : qtree(id,v,0,0,ty) {} 

 };

#define DECLARE_ZEROZERO_QTREE(type) struct qtree_##type : public qtree_zerozero  { qtree_##type(long id, const qqchar& v) : qtree_zerozero(id,v,type) {} };  EXTERN_NEW_QTREE_TYPE(type);

 // ut8, utf8 doulbe quoted strings
DECLARE_ZEROZERO_QTREE(t_ut8)


// doesn't work: consumes ')' close parens as children: DECLARE_PREFIX_QTREE(t_ut8,10)

// DECLARE_QTREE_NEW(t_comma,0)
DECLARE_ZEROZERO_QTREE(t_comma)

 struct qtree_number : public qtree      { 
     qtree_number(long id, const qqchar& v) : qtree(id,v,0,0,t_ato) {} 
 };
EXTERN_NEW_QTREE_TYPE(number);



 // DECLARE_QTREE_NEW(t_c_static,0)
 // DECLARE_QTREE_NEW(t_c_volatile,0)
 // DECLARE_QTREE_NEW(t_c_int,0)


 // test some prefix tokens, like declaration and type keywords in C: static, voltile, int 
 struct qtree_t_c_static : public qtree_prefix { 
     qtree_t_c_static(long id, const qqchar& v) : qtree_prefix(id,v, 10, t_c_static) {} 
 };
EXTERN_NEW_QTREE_TYPE(t_c_static);

 struct qtree_t_c_volatile : public qtree_prefix { 
  qtree_t_c_volatile(long id, const qqchar& v) : qtree_prefix(id,v, 10, t_c_volatile) {} 
 };
EXTERN_NEW_QTREE_TYPE(t_c_volatile);

 struct qtree_t_c_int : public qtree_prefix { 
  qtree_t_c_int(long id, const qqchar& v) : qtree_prefix(id,v, 10, t_c_int) {} 
 };
EXTERN_NEW_QTREE_TYPE(t_c_int);

//
// variable declarations, t_dcl
//
struct qtree_t_dcl : public qtree { 

     qtree_t_dcl(long id, const qqchar& v) : qtree(id,v, 0, 70, t_dcl) {
         munch_right = &prefix_munch_right;
     } 
 };
EXTERN_NEW_QTREE_TYPE(t_dcl);




// define DECLARE_QTREE_NEW(type,prec) struct qtree_##type : public qtree  { qtree_##type* clone() const  { return new qtree_##type(*this); }  qtree_##type(long id, const qqchar& v) : qtree(id,v) { _ty=type; } };  extern qtree*  new_##type(long id, const qqchar& v);

#define DECLARE_QTREE_NEW(type,prec) struct qtree_##type : public qtree  {  qtree_##type(long id, const qqchar& v) : qtree(id,v) { _ty=type; } };  EXTERN_NEW_QTREE_TYPE(type)



// struct qtree_t_dou : public qtree_number {  qtree_t_dou(long id, const qqchar& v) : qtree_number(id,v) {} };

// 't_dou' is lexing as 't_dou', but then if constructor sets t_ato, we have a problem. So back out the qtree_number
//   stuff until we get the old tests working... and have some new tests written for number.

DECLARE_ZEROZERO_QTREE(t_dou)
//DECLARE_PREFIX_QTREE(t_dou,0)
//EXTERN_NEW_QTREE_TYPE(t_dou);

DECLARE_ZEROZERO_QTREE(t_ref)
//DECLARE_PREFIX_QTREE(t_ref,0)
//EXTERN_NEW_QTREE_TYPE(t_ref);



 // math types


 DECLARE_INFIX_QTREE(m_eql, 40); //  eq (better ==)
 DECLARE_INFIX_QTREE(m_eqe, 40); //  ==
 DECLARE_INFIX_QTREE(m_dvn, 60); //  /
 DECLARE_INFIX_QTREE(m_mlt, 60); //  *
 DECLARE_INFIX_QTREE(m_pls, 50); //  +



     // unary minus sign
qtree* minus_munch_right(qtree* _this_);

qtree* minus_munch_left(qtree* _this_, qtree* left);


//
//  -   : has both unary prefix, and infix binary operator duties...
//
 struct qtree_m_mns : public qtree { 
    qtree_m_mns(long id, const qqchar& v) : qtree(id,v, 50, 70, m_mns) {
        munch_right  = &minus_munch_right;
        munch_left   = &minus_munch_left;
    }
 };
EXTERN_NEW_QTREE_TYPE(m_mns);

 // bit wise or |  and bit wise and &
 DECLARE_INFIX_QTREE(t_and,30)
 DECLARE_INFIX_QTREE(m_bar,30)


 //
 // [ ]   : t_osq t_csq
 //
 struct qtree_t_osq : public qtree_group { 


     qtree_t_osq(long id, const qqchar& v) : qtree_group(id,v, 80, 0, t_osq, t_csq) {
         munch_left  = &group_with_headnode_munch_left;
         munch_right = &group_munch_right;
     } 

#if 0
     void print(const char* indent = "") {
         l3path im(0,"%s      ",indent);
         l3path im2(0,"%s      ",im());
         printf("%sAREF t_osq %p\n",indent,this);
         printf("%s[begin  %p._array]\n",im(),this);
         if (_headnode) { 
             _headnode->print(im2());
         } else {
             printf("%s_array = 0x0\n", im2());
         }
         printf("%s[end of %p._array]\n",im(),this);

         qtree::print(indent);
     }
#endif


 };
EXTERN_NEW_QTREE_TYPE(t_osq);

 //
 //  humble ;
 //

qtree* stopper_munch_left(qtree* _this_, qtree* left);


 struct qtree_t_semicolon : public qtree {

     qtree_t_semicolon(long id, const qqchar& v) : qtree(id,v, 0,0, t_semicolon) {
         munch_left = &stopper_munch_left;
     }
 };
EXTERN_NEW_QTREE_TYPE(t_semicolon);

 //
 // }  t_cbr
 // 
 struct qtree_t_cbr : public qtree {

     qtree_t_cbr(long id, const qqchar& v) : qtree(id,v, 0,0, t_cbr) {
         munch_left = &stopper_munch_left;
     }
 };
EXTERN_NEW_QTREE_TYPE(t_cbr);


#if 0 // the colon operator for indexing is way more important, i.e. 1:10 to index from 1 to 10.x
 //
 // ? : ternary operator
 //

qtree* ternary_munch_left(qtree* _this_, qtree* left);

struct qtree_m_que : public qtree { 
    qtree_m_que(long id, const qqchar& v) : qtree(id,v, 20, 20, m_que) {
        munch_left  =  &ternary_munch_left;
    } 
 };
 EXTERN_NEW_QTREE_TYPE(m_que);
#else
// so just make it a stopper/ atom like symbol:
DECLARE_ZEROZERO_QTREE(m_que)

#endif

struct qtree_stmt : public qtree {};

//
// while
//

qtree* while_munch_right(qtree* _this_);


struct qtree_t_c_while : public qtree { 

    //    qtree_t_c_while* clone() const  { return new qtree_t_c_while(*this); } 

    qtree_t_c_while(long id, const qqchar& v) : qtree(id,v, 0, 0, t_c_while) {
        munch_right =  &while_munch_right;
    }     
};
EXTERN_NEW_QTREE_TYPE(t_c_while);



//
// if
//
qtree* c_if_munch_right(qtree* _this_);


struct qtree_t_c_if : public qtree { 


 qtree_t_c_if(long id, const qqchar& v) : qtree(id,v, 0, 0, t_c_if) {
     munch_right = &c_if_munch_right;
 } 

};
EXTERN_NEW_QTREE_TYPE(t_c_if);


//
// for loop : t_c_for
//


qtree* c_for_munch_right(qtree* _this_);


struct qtree_t_c_for : public qtree { 
    qtree_t_c_for(long id, const qqchar& v) : qtree(id,v, 0, 0, t_c_for) {
        munch_right = &c_for_munch_right;
    }
};
EXTERN_NEW_QTREE_TYPE(t_c_for);



//
// ++ m_ppl
// 

qtree* plusplus_munch_left(qtree* _this_, qtree* left);

qtree* plusplus_munch_right(qtree* _this_);

struct qtree_m_ppl : public qtree { 

    qtree_m_ppl(long id, const qqchar& v) : qtree(id,v, 80, 70, m_ppl) {
        munch_right = &plusplus_munch_right;
        munch_left  = &plusplus_munch_left;
    } 

#if 0
     void stringify_prechildren(l3bigstr* d, const char* ind) {
         l3path im(0,"%s        ",(char*)ind);
         l3path nim(0,"\n%s",im());
         
         if (_postfix) {
             if (ind) { d->pushf("%s<postfix:>", nim()); } else { d->pushf(" postfix:");}
             _postfix->stringify(d, ind ? im() : 0);
             if (ind) { d->pushf("%s</postfix>", nim()); } 
         }
         
         if (_prefix) {
             if (ind) { d->pushf("%s<prefix:>", nim()); } else { d->pushf(" prefix:");}
             _prefix->stringify(d, ind ? im() : 0);
             if (ind) { d->pushf("%s</prefix>", nim()); } 
         }
     }
#endif
    
};
EXTERN_NEW_QTREE_TYPE(m_ppl);


//
// ++ m_mmn
// 
struct qtree_m_mmn : public qtree { 


 qtree_m_mmn(long id, const qqchar& v) : qtree(id,v, 80, 70, m_mmn) {
     munch_right = &plusplus_munch_right;
     munch_left  = &plusplus_munch_left;
 } 

    
};
EXTERN_NEW_QTREE_TYPE(m_mmn);







 //
 // infix relational operations: >, <, >=, <=, !=
 //
 DECLARE_INFIX_QTREE(m_gth,40)
 DECLARE_INFIX_QTREE(m_lth,40)
 DECLARE_INFIX_QTREE(m_gte,40)
 DECLARE_INFIX_QTREE(m_lte,40)
 DECLARE_INFIX_QTREE(m_neq,40)

 DECLARE_INFIX_QTREE(m_cln,40) // :

 // not addressed yet, just boilerplate:

 DECLARE_QTREE_NEW(m_crt,10)
 DECLARE_QTREE_NEW(m_mod,10)


 DECLARE_QTREE_NEW(m_umi,10)


 DECLARE_QTREE_NEW(m_ats,10)
 DECLARE_QTREE_NEW(m_hsh,10)
 DECLARE_QTREE_NEW(m_dlr,10)
 DECLARE_QTREE_NEW(m_pct,10)
 //  DECLARE_QTREE_NEW(m_cln,10) // above


 // DECLARE_QTREE_NEW(m_ppl,10) // above
 // DECLARE_QTREE_NEW(m_mmn,10) // above


 // protobuf keywords

 DECLARE_PREFIX_QTREE(t_pb_message,10)
 DECLARE_PREFIX_QTREE(t_pb_import,10)
 DECLARE_PREFIX_QTREE(t_pb_package,10)
 DECLARE_PREFIX_QTREE(t_pb_service,10)
 DECLARE_PREFIX_QTREE(t_pb_rpc,10)
 DECLARE_PREFIX_QTREE(t_pb_extensions,10)
 DECLARE_PREFIX_QTREE(t_pb_extend,10)
 DECLARE_PREFIX_QTREE(t_pb_option,10)
 DECLARE_PREFIX_QTREE(t_pb_required,10)
 DECLARE_PREFIX_QTREE(t_pb_repeated,10)
 DECLARE_PREFIX_QTREE(t_pb_optional,10)
 DECLARE_PREFIX_QTREE(t_pb_enum,10)

 DECLARE_PREFIX_QTREE(t_pby_int32,10)
 DECLARE_PREFIX_QTREE(t_pby_int64,10)
 DECLARE_PREFIX_QTREE(t_pby_uint32,10)
 DECLARE_PREFIX_QTREE(t_pby_uint64,10)
 DECLARE_PREFIX_QTREE(t_pby_sint32,10)
 DECLARE_PREFIX_QTREE(t_pby_sint64,10)
 DECLARE_PREFIX_QTREE(t_pby_fixed32,10)
 DECLARE_PREFIX_QTREE(t_pby_fixed64,10)
 DECLARE_PREFIX_QTREE(t_pby_sfixed32,10)
 DECLARE_PREFIX_QTREE(t_pby_sfixed64,10)
 DECLARE_PREFIX_QTREE(t_pby_string,10)
 DECLARE_PREFIX_QTREE(t_pby_bytes,10)


 // ; \n // , 

 DECLARE_QTREE_NEW(t_newline,0)

 //
 //  t_cppcomment : //
 //


qtree* t_cppcomment_munch_right(qtree* _this_);

struct qtree_t_cppcomment : public qtree  {  

    qtree_t_cppcomment(long id, const qqchar& v) : qtree(id,v) { 
        _ty=t_cppcomment; 

        munch_right = &t_cppcomment_munch_right;
    } 

};  
EXTERN_NEW_QTREE_TYPE(t_cppcomment)

 // =  { } [ ]  )



 DECLARE_QTREE_NEW(t_csq,10)


 // t_chr : single character in single quotes.
 DECLARE_QTREE_NEW(t_chr,0)



 // DECLARE_PREFIX_QTREE(t_oso,10)
 // DECLARE_PREFIX_QTREE(t_ico,10)
 // DECLARE_PREFIX_QTREE(t_iso,10)
 // DECLARE_PREFIX_QTREE(t_oco,10)

 DECLARE_ZEROZERO_QTREE(t_oso)
 DECLARE_ZEROZERO_QTREE(t_ico)
 DECLARE_ZEROZERO_QTREE(t_iso)
 DECLARE_ZEROZERO_QTREE(t_oco)




 //
 // C/C99 and C++ keywords
 //

 // prefix keywords

 DECLARE_PREFIX_QTREE(t_c_auto,10)
 DECLARE_PREFIX_QTREE(t_c_double,70)
 //DECLARE_PREFIX_QTREE(t_c_int,10) // fleshed out above.
 DECLARE_PREFIX_QTREE(t_c_long,10)
 DECLARE_PREFIX_QTREE(t_c_register,10)
 DECLARE_PREFIX_QTREE(t_c_char,10)
 DECLARE_PREFIX_QTREE(t_c_extern,10)
 DECLARE_PREFIX_QTREE(t_c_const,10)
 DECLARE_PREFIX_QTREE(t_c_float,10)
 DECLARE_PREFIX_QTREE(t_c_short,10)
 DECLARE_PREFIX_QTREE(t_c_unsigned,10)
 DECLARE_PREFIX_QTREE(t_c_signed,10)
 DECLARE_PREFIX_QTREE(t_c_void,10)
 DECLARE_PREFIX_QTREE(t_c__Bool,10)
 DECLARE_PREFIX_QTREE(t_c_inline,10)
 DECLARE_PREFIX_QTREE(t_c__Complex,10)
 DECLARE_PREFIX_QTREE(t_c_restrict,10)
 DECLARE_PREFIX_QTREE(t_c__Imaginary,10)

 DECLARE_PREFIX_QTREE(t_cc_friend,10)
 DECLARE_PREFIX_QTREE(t_cc_private,10)
 DECLARE_PREFIX_QTREE(t_cc_public,10)
 DECLARE_PREFIX_QTREE(t_cc_virtual,10)
 DECLARE_PREFIX_QTREE(t_cc_mutable,10)
 DECLARE_PREFIX_QTREE(t_cc_protected,10)
 DECLARE_PREFIX_QTREE(t_cc_bool,10)
 DECLARE_PREFIX_QTREE(t_cc_wchar_t,10)

 DECLARE_PREFIX_QTREE(t_c_else,10) 

 DECLARE_PREFIX_QTREE(t_vec,10)

 // DECLARE_QTREE_NEW(t_c_for,0) // above


 DECLARE_QTREE_NEW(t_c_break,0)
 DECLARE_QTREE_NEW(t_c_continue,0)
 DECLARE_PREFIX_QTREE(t_c_return,10)
 DECLARE_PREFIX_QTREE(t_c_goto,10)
 DECLARE_QTREE_NEW(t_c_default,0)



 DECLARE_PREFIX_QTREE(t_c_struct,10)
 DECLARE_PREFIX_QTREE(t_c_switch,10)
 DECLARE_QTREE_NEW(t_c_case,0)
 //DECLARE_QTREE_NEW(t_c_enum,0) // already in protobuf words above
 DECLARE_QTREE_NEW(t_c_typedef,0)
 DECLARE_QTREE_NEW(t_c_union,0)


 DECLARE_QTREE_NEW(t_c_sizeof,0)
 //DECLARE_QTREE_NEW(t_c_volatile,0)
 DECLARE_QTREE_NEW(t_c_do,0)
 // DECLARE_QTREE_NEW(t_c_if,0) // above now
 //DECLARE_QTREE_NEW(t_c_static,0)
 // DECLARE_QTREE_NEW(t_c_while,0) // above
 DECLARE_QTREE_NEW(t_cc_asm,0)
 DECLARE_QTREE_NEW(t_cc_dynamic_cast,0)
 DECLARE_QTREE_NEW(t_cc_namespace,0)
 DECLARE_QTREE_NEW(t_cc_reinterpret_cast,0)
 DECLARE_QTREE_NEW(t_cc_explicit,0)
 DECLARE_QTREE_NEW(t_cc_new,0)
 DECLARE_QTREE_NEW(t_cc_static_cast,0)
 DECLARE_QTREE_NEW(t_cc_typeid,0)

 // let these be atoms, for now.
 DECLARE_ZEROZERO_QTREE(t_cc_try)
 DECLARE_ZEROZERO_QTREE(t_cc_catch)
 DECLARE_ZEROZERO_QTREE(t_cc_throw)

 DECLARE_QTREE_NEW(t_cc_false,0)
 DECLARE_QTREE_NEW(t_cc_operator,0)
 DECLARE_QTREE_NEW(t_cc_template,0)
 DECLARE_QTREE_NEW(t_cc_typename,0)
 DECLARE_PREFIX_QTREE(t_cc_class,10)
 DECLARE_QTREE_NEW(t_cc_this,0)
 DECLARE_QTREE_NEW(t_cc_using,0)
 DECLARE_QTREE_NEW(t_cc_const_cast,0)

 DECLARE_QTREE_NEW(t_cc_delete,0)
 DECLARE_QTREE_NEW(t_cc_true,0)
 DECLARE_QTREE_NEW(t_cc_and,0)
 DECLARE_QTREE_NEW(t_cc_bitand,0)
 DECLARE_QTREE_NEW(t_cc_compl,0)
 DECLARE_QTREE_NEW(t_cc_not_eq,0)
 DECLARE_QTREE_NEW(t_cc_or_eq,0)
 DECLARE_QTREE_NEW(t_cc_xor_eq,0)
 DECLARE_QTREE_NEW(t_cc_and_eq,0)
 DECLARE_QTREE_NEW(t_cc_bitor,0)
 DECLARE_QTREE_NEW(t_cc_not,0)
 DECLARE_QTREE_NEW(t_cc_or,0)
 DECLARE_QTREE_NEW(t_cc_xor,0)

 DECLARE_QTREE_NEW(t_any,0)
 DECLARE_QTREE_NEW(t_eof,0)


 L3FORWDECL(tdop)
 L3FORWDECL(addtxt) // add more text to a tdop parser.
 qtree*  tdop_get_parsed(l3obj* tdop_obj);

//
// a tdop object will encapsulate parsing and output, using the tdopout_struct here:
//

struct tdopout_struct {

     // stuff used by the qtreefactory by reference:
     vector<char*>           _tk;
     vector<char*>           _tkend;
     vector<t_typ>           _tky;
     vector<ptr2new_qtree>   _tksem;
     vector<long>            _extendsto;
     vector<bool>            _ignore;

     std::string             _output_stringified_viewable;
     std::string             _output_stringified_compressed;

     qtreefactory   _qf;

     l3obj*         _myobj;


    // the final output parsed tree, the last one..
    qtree*           _parsed;

    ustaq<qtree>    _parsed_ustaq; // hold the series of _parsed, to handle multiple expressions per segment.

    // allow partial/incomplete parses (turns off the paren balance checks)
    bool             _partialokay;

    vector<char*>        _vstart;      // from multiple calls (addtxt calls after ctor); each _start is stored here.
    long                 _k;           // which one we are on... so _vstart[_k] should effectively replace _start.
    vector<long>         _firstid;    // _firstid[k] gives the number of the first token that came from text _vstart[k]

    vector<bool>         _vis_mmap;    // generalize _is_mmap
    vector<std::string>  _vfname;      // generalize _fname to multiple segments
    vector<int>          _vfd;         // generalize _fd    to multiple segments

    // adopt the convention that negative sz means permatext for _vampsz:
    vector<long>         _vmapsz;      // generalize _mapsz to multiple segments

    
    // if fname is given, then start should be 0x0 and we will mmap the file and set start from that. We copy to std::string internally.
    // if start is not mmap then we will ::free() it in the dtor.
    //
 tdopout_struct(char* start, const char* fname, bool start_is_mmap, long start_mapsz)
 :
        _qf(_tk,_tkend,_tky,_tksem, _extendsto, _ignore, this),
        _parsed(0),
        _partialokay(true),
        _k(0)
     {
         _vstart.push_back(start);
         _vfname.push_back(fname);
         _vmapsz.push_back(start_mapsz);
         _vis_mmap.push_back(start_is_mmap);
         _vfd.push_back(-1);
         _firstid.push_back(0);
     }

    ~tdopout_struct();

    // copy constructor cpctor
    tdopout_struct(const tdopout_struct& src) :
        _qf(_tk,
            _tkend,
            _tky,
            _tksem,
            _extendsto,
            _ignore,
            this)
    {
        CopyFrom(src);
    }

    inline tdopout_struct& operator=(const tdopout_struct& src) {
        if (&src != this) {
            CopyFrom(src);
        }
        return *this;
    }
        
    void CopyFrom(const tdopout_struct& src) {

        ulong Nseg =   src._firstid.size();
        assert(Nseg); // have to have at least one.

        // sanity checks ; 6 arrays
        assert(Nseg == src._firstid.size()); // duh.
        assert(Nseg == src._vstart.size());
        assert(Nseg == src._vfname.size());

        assert(Nseg == src._vfd.size());
        assert(Nseg == src._vmapsz.size());
        assert(Nseg == src._vis_mmap.size());


        // 6 segment arrays to copy, here are 5:
        _vmapsz    = src._vmapsz;
        _firstid   = src._firstid;
        _vfd.resize(Nseg, -1);
        _vfname.resize(Nseg, "(internal string)");
        _vis_mmap.resize(Nseg, false);

        // and the last one is the main one, the _vstart[] contents:
        
        // get the size right.
        _vstart.resize(Nseg);

        // and copy the vstart contents
        for (ulong i = 0; i < _vstart.size(); ++i) {

            // tdop no longer owns what it points to. permatext owns it, just point to it.
            _vstart[i] = src._vstart[i];
        }


        // need to adjust the char pointers here:
        _tk    = src._tk;      // vector<char*>           _tk;
        _tkend = src._tkend;   // vector<char*>           _tkend;

        // now copy the token arrays, adjusting _tk and _tkend.

        if (Nseg == 1) {

            // one and only segment
            long offset = _vstart[0] - src._vstart[0];
            ulong N_tok_this_seg = _tk.size();
            for (ulong i =0; i < N_tok_this_seg; ++i) {
                _tk[i]    += offset;
                _tkend[i] += offset;
            }

        } else {
            // more than one segment
            assert(Nseg > 1);

            // add to _first_id so that _firstid[j] .. _firstid[j+1] gives the [start..end) range to adjust
            _firstid.push_back(_tk.size());

            for (ulong j =0; j < Nseg; ++j) {
                
                long offset = _vstart[j] - src._vstart[j];

                long start = _firstid[j];
                assert(start >= 0);
                long stop  = _firstid[j+1];
                assert(stop  >= 0);
                assert(stop >= start);

                long N_tok_this_seg = stop - start;
                assert(N_tok_this_seg >= 0);

                for (long i =0; i < N_tok_this_seg; ++i) {
                    _tk[start + i]    += offset;
                    _tkend[start + i] += offset;
                }

                // put _firstid back to normal.
                _firstid.pop_back();
            }
        }

        _tky   = src._tky; // ok - nothing further needed.
        _tksem = src._tksem; // ok
        _extendsto = src._extendsto; // ok
        _ignore    = src._ignore; // ok 

        _partialokay = src._partialokay; // ok
        _k = src._k;                     // ok

        // should not need anything: _parsed_ustaq;

        // the final output parsed tree
        // easier to reparse that to try and clone. sets _parsed:
        parse_cmd cmd;
        and_parse(&cmd);

        // already set by cpctor: _myobj 
        
    }

    void save_expr(qtree* parsed, parse_cmd* client_parse_cmd);

    void reset() {

        _tk.clear();
        _tkend.clear();
        _tky.clear();
        _tksem.clear();

        _extendsto.clear();
        _ignore.clear();

        _output_stringified_viewable = "";
        _output_stringified_compressed = "";

        _qf.reset();

        _parsed = 0;
        _parsed_ustaq.clear();

        _vstart.clear();
        _k =0;
        _firstid.clear();
        _vis_mmap.clear();
        _vfname.clear();
        _vfd.clear();
        _vmapsz.clear();

    }

    void init_mmap_from_file();

    // pushd pointers to results onto clientstk and our _parsed_ustaq
    long lex_and_parse(parse_cmd* client_parse_cmd);

    // called by lex_and_parse(), and by 
    void update_extendsto_and_ignore();

    // just the parsing part
    long and_parse(parse_cmd* client_parse_cmd);


    // get the segment index for a given token, using _firstid to lookup.
    long which_segment(long tokid) 
    {
        assert(tokid < (long)_tk.size());
        
        // first and only?
        long N = (long)_firstid.size();
        if (N==1) { return 0; }
        
        // linear search
        for (long i = N-1; i >=0 ; --i) {
            if (tokid >= _firstid[i]) {
                return i;
            }
        }

        printf("error in tdopout_struct::which_segment() : could not find index for tokid %ld.\n",tokid);
        assert(0);
        exit(1);
        return 0;
    }

    void print_orig(int fd = 0) {

        ulong M = _firstid.size(); 
        long stopi  = 0;

        for (ulong i =0 ; i < M; ++i) {
            long starti = _firstid[i];

            if (M > 1 && i < M-1) {
                stopi = _firstid[i+1]-1;
            } else {
                stopi  = _tk.size()-1;
            }

            write(fd, _tk[starti], _tkend[stopi] - _tk[starti]);
            write(fd, " ", 1);
        }
        write(fd, "\n", 1);
    }

    //
    // copy a span of tokens into the buffer starting at s
    //
    long copyto(char* s, ulong cap, const tokenspan& ts) {
        return _qf.copyto(s,cap,ts);
    }


    void print_sexp() {
            l3bigstr d;
            _parsed->sexp_string(&d,0);
            d.outln();
    }

    const char* filename(long tokid) {
        long i = which_segment(tokid);
        return _vfname[i].c_str();
    }


    long add_string_segment(const char* p, long sz);
    void backup_one_elim_eof();
    void  backup_one_elim_eof_for_addstring();
    void serialize_to(l3path& fname);
    void unserialize_from(l3path& fname);

   
};

void tdo_print(l3obj* obj, const char* indent, stopset* stoppers);


void     parse_sexp(char *s, size_t len, Tag* ptag, parse_cmd& cmd, long* nchar_consumed, l3obj* env, l3obj** retval_tdop, FILE* ifp);
sexp_t*  copy_sexp(sexp_t *sx, Tag* ptag);

void destroy_sexp(sexp_t* s);
void  recursively_destroy_sexp(qtree* sx, Tag* tag);

bool assert_sexp_all_owned_by_tag(qtree* exp, Tag* ptag);
void sexpobj_print(l3obj* obj,const char* indent, stopset* stoppers);


L3FORWDECL(tdo_save)
L3FORWDECL(tdo_load)

void test_pratt();

void recursively_set_notsysbuiltin(qtree* exp);
void recursively_set_sysbuiltin(qtree* exp);

void print_ustaq_of_sexp_as_outline(ustaq<sexp_t>& sexpstack, const char* indent);


#endif /* L3PRATT_H */

