// A Bison parser, made by GNU Bison 3.8.2.

// Skeleton implementation for Bison LALR(1) parsers in C++

// Copyright (C) 2002-2015, 2018-2021 Free Software Foundation, Inc.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// As a special exception, you may create a larger work that contains
// part or all of the Bison parser skeleton and distribute that work
// under terms of your choice, so long as that work isn't itself a
// parser generator using the skeleton or a modified version thereof
// as a parser skeleton.  Alternatively, if you modify or redistribute
// the parser skeleton itself, you may (at your option) remove this
// special exception, which will cause the skeleton and the resulting
// Bison output files to be licensed under the GNU General Public
// License without this special exception.

// This special exception was added by the Free Software Foundation in
// version 2.2 of Bison.

// DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
// especially those whose name start with YY_ or yy_.  They are
// private implementation details that can be changed or removed.





#include "script.hpp"


// Unqualified %code blocks.
#line 11 "src/link/script.y"

	#include <algorithm>
	#include <array>
	#include <assert.h>
	#include <cinttypes>
	#include <fstream>
	#include <locale>
	#include <string_view>
	#include <vector>

	#include "itertools.hpp"
	#include "util.hpp"

	#include "link/main.hpp"
	#include "link/section.hpp"

	using namespace std::literals;

	static void includeFile(std::string &&path);
	static void incLineNo(void);

	static void setSectionType(SectionType type);
	static void setSectionType(SectionType type, uint32_t bank);
	static void setAddr(uint32_t addr);
	static void alignTo(uint32_t alignment, uint32_t offset);
	static void pad(uint32_t length);
	static void placeSection(std::string const &name);

	static yy::parser::symbol_type yylex(void);

	struct Keyword {
		std::string_view name;
		yy::parser::symbol_type (* tokenGen)(void);
	};
#line 52 "src/link/script.y"

	static std::array keywords{
		Keyword{"ORG"sv,     yy::parser::make_ORG},
		Keyword{"INCLUDE"sv, yy::parser::make_INCLUDE},
		Keyword{"ALIGN"sv,   yy::parser::make_ALIGN},
		Keyword{"DS"sv,      yy::parser::make_DS},
	};

#line 90 "src/link/script.cpp"


#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> // FIXME: INFRINGES ON USER NAME SPACE.
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif


// Whether we are compiled with exception support.
#ifndef YY_EXCEPTIONS
# if defined __GNUC__ && !defined __EXCEPTIONS
#  define YY_EXCEPTIONS 0
# else
#  define YY_EXCEPTIONS 1
# endif
#endif



// Enable debugging if requested.
#if YYDEBUG

// A pseudo ostream that takes yydebug_ into account.
# define YYCDEBUG if (yydebug_) (*yycdebug_)

# define YY_SYMBOL_PRINT(Title, Symbol)         \
  do {                                          \
    if (yydebug_)                               \
    {                                           \
      *yycdebug_ << Title << ' ';               \
      yy_print_ (*yycdebug_, Symbol);           \
      *yycdebug_ << '\n';                       \
    }                                           \
  } while (false)

# define YY_REDUCE_PRINT(Rule)          \
  do {                                  \
    if (yydebug_)                       \
      yy_reduce_print_ (Rule);          \
  } while (false)

# define YY_STACK_PRINT()               \
  do {                                  \
    if (yydebug_)                       \
      yy_stack_print_ ();                \
  } while (false)

#else // !YYDEBUG

# define YYCDEBUG if (false) std::cerr
# define YY_SYMBOL_PRINT(Title, Symbol)  YY_USE (Symbol)
# define YY_REDUCE_PRINT(Rule)           static_cast<void> (0)
# define YY_STACK_PRINT()                static_cast<void> (0)

#endif // !YYDEBUG

#define yyerrok         (yyerrstatus_ = 0)
#define yyclearin       (yyla.clear ())

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYRECOVERING()  (!!yyerrstatus_)

namespace yy {
#line 163 "src/link/script.cpp"

  /// Build a parser object.
  parser::parser ()
#if YYDEBUG
    : yydebug_ (false),
      yycdebug_ (&std::cerr),
#else
    :
#endif
      yy_lac_established_ (false)
  {}

  parser::~parser ()
  {}

  parser::syntax_error::~syntax_error () YY_NOEXCEPT YY_NOTHROW
  {}

  /*---------.
  | symbol.  |
  `---------*/



  // by_state.
  parser::by_state::by_state () YY_NOEXCEPT
    : state (empty_state)
  {}

  parser::by_state::by_state (const by_state& that) YY_NOEXCEPT
    : state (that.state)
  {}

  void
  parser::by_state::clear () YY_NOEXCEPT
  {
    state = empty_state;
  }

  void
  parser::by_state::move (by_state& that)
  {
    state = that.state;
    that.clear ();
  }

  parser::by_state::by_state (state_type s) YY_NOEXCEPT
    : state (s)
  {}

  parser::symbol_kind_type
  parser::by_state::kind () const YY_NOEXCEPT
  {
    if (state == empty_state)
      return symbol_kind::S_YYEMPTY;
    else
      return YY_CAST (symbol_kind_type, yystos_[+state]);
  }

  parser::stack_symbol_type::stack_symbol_type ()
  {}

  parser::stack_symbol_type::stack_symbol_type (YY_RVREF (stack_symbol_type) that)
    : super_type (YY_MOVE (that.state))
  {
    switch (that.kind ())
    {
      case symbol_kind::S_section_type: // section_type
        value.YY_MOVE_OR_COPY< SectionType > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_string: // string
        value.YY_MOVE_OR_COPY< std::string > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_number: // number
        value.YY_MOVE_OR_COPY< uint32_t > (YY_MOVE (that.value));
        break;

      default:
        break;
    }

#if 201103L <= YY_CPLUSPLUS
    // that is emptied.
    that.state = empty_state;
#endif
  }

  parser::stack_symbol_type::stack_symbol_type (state_type s, YY_MOVE_REF (symbol_type) that)
    : super_type (s)
  {
    switch (that.kind ())
    {
      case symbol_kind::S_section_type: // section_type
        value.move< SectionType > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_string: // string
        value.move< std::string > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_number: // number
        value.move< uint32_t > (YY_MOVE (that.value));
        break;

      default:
        break;
    }

    // that is emptied.
    that.kind_ = symbol_kind::S_YYEMPTY;
  }

#if YY_CPLUSPLUS < 201103L
  parser::stack_symbol_type&
  parser::stack_symbol_type::operator= (const stack_symbol_type& that)
  {
    state = that.state;
    switch (that.kind ())
    {
      case symbol_kind::S_section_type: // section_type
        value.copy< SectionType > (that.value);
        break;

      case symbol_kind::S_string: // string
        value.copy< std::string > (that.value);
        break;

      case symbol_kind::S_number: // number
        value.copy< uint32_t > (that.value);
        break;

      default:
        break;
    }

    return *this;
  }

  parser::stack_symbol_type&
  parser::stack_symbol_type::operator= (stack_symbol_type& that)
  {
    state = that.state;
    switch (that.kind ())
    {
      case symbol_kind::S_section_type: // section_type
        value.move< SectionType > (that.value);
        break;

      case symbol_kind::S_string: // string
        value.move< std::string > (that.value);
        break;

      case symbol_kind::S_number: // number
        value.move< uint32_t > (that.value);
        break;

      default:
        break;
    }

    // that is emptied.
    that.state = empty_state;
    return *this;
  }
#endif

  template <typename Base>
  void
  parser::yy_destroy_ (const char* yymsg, basic_symbol<Base>& yysym) const
  {
    if (yymsg)
      YY_SYMBOL_PRINT (yymsg, yysym);
  }

#if YYDEBUG
  template <typename Base>
  void
  parser::yy_print_ (std::ostream& yyo, const basic_symbol<Base>& yysym) const
  {
    std::ostream& yyoutput = yyo;
    YY_USE (yyoutput);
    if (yysym.empty ())
      yyo << "empty symbol";
    else
      {
        symbol_kind_type yykind = yysym.kind ();
        yyo << (yykind < YYNTOKENS ? "token" : "nterm")
            << ' ' << yysym.name () << " (";
        YY_USE (yykind);
        yyo << ')';
      }
  }
#endif

  void
  parser::yypush_ (const char* m, YY_MOVE_REF (stack_symbol_type) sym)
  {
    if (m)
      YY_SYMBOL_PRINT (m, sym);
    yystack_.push (YY_MOVE (sym));
  }

  void
  parser::yypush_ (const char* m, state_type s, YY_MOVE_REF (symbol_type) sym)
  {
#if 201103L <= YY_CPLUSPLUS
    yypush_ (m, stack_symbol_type (s, std::move (sym)));
#else
    stack_symbol_type ss (s, sym);
    yypush_ (m, ss);
#endif
  }

  void
  parser::yypop_ (int n) YY_NOEXCEPT
  {
    yystack_.pop (n);
  }

#if YYDEBUG
  std::ostream&
  parser::debug_stream () const
  {
    return *yycdebug_;
  }

  void
  parser::set_debug_stream (std::ostream& o)
  {
    yycdebug_ = &o;
  }


  parser::debug_level_type
  parser::debug_level () const
  {
    return yydebug_;
  }

  void
  parser::set_debug_level (debug_level_type l)
  {
    yydebug_ = l;
  }
#endif // YYDEBUG

  parser::state_type
  parser::yy_lr_goto_state_ (state_type yystate, int yysym)
  {
    int yyr = yypgoto_[yysym - YYNTOKENS] + yystate;
    if (0 <= yyr && yyr <= yylast_ && yycheck_[yyr] == yystate)
      return yytable_[yyr];
    else
      return yydefgoto_[yysym - YYNTOKENS];
  }

  bool
  parser::yy_pact_value_is_default_ (int yyvalue) YY_NOEXCEPT
  {
    return yyvalue == yypact_ninf_;
  }

  bool
  parser::yy_table_value_is_error_ (int yyvalue) YY_NOEXCEPT
  {
    return yyvalue == yytable_ninf_;
  }

  int
  parser::operator() ()
  {
    return parse ();
  }

  int
  parser::parse ()
  {
    int yyn;
    /// Length of the RHS of the rule being reduced.
    int yylen = 0;

    // Error handling.
    int yynerrs_ = 0;
    int yyerrstatus_ = 0;

    /// The lookahead symbol.
    symbol_type yyla;

    /// The return value of parse ().
    int yyresult;

    // Discard the LAC context in case there still is one left from a
    // previous invocation.
    yy_lac_discard_ ("init");

#if YY_EXCEPTIONS
    try
#endif // YY_EXCEPTIONS
      {
    YYCDEBUG << "Starting parse\n";


    /* Initialize the stack.  The initial state will be set in
       yynewstate, since the latter expects the semantical and the
       location values to have been already stored, initialize these
       stacks with a primary value.  */
    yystack_.clear ();
    yypush_ (YY_NULLPTR, 0, YY_MOVE (yyla));

  /*-----------------------------------------------.
  | yynewstate -- push a new symbol on the stack.  |
  `-----------------------------------------------*/
  yynewstate:
    YYCDEBUG << "Entering state " << int (yystack_[0].state) << '\n';
    YY_STACK_PRINT ();

    // Accept?
    if (yystack_[0].state == yyfinal_)
      YYACCEPT;

    goto yybackup;


  /*-----------.
  | yybackup.  |
  `-----------*/
  yybackup:
    // Try to take a decision without lookahead.
    yyn = yypact_[+yystack_[0].state];
    if (yy_pact_value_is_default_ (yyn))
      goto yydefault;

    // Read a lookahead token.
    if (yyla.empty ())
      {
        YYCDEBUG << "Reading a token\n";
#if YY_EXCEPTIONS
        try
#endif // YY_EXCEPTIONS
          {
            symbol_type yylookahead (yylex ());
            yyla.move (yylookahead);
          }
#if YY_EXCEPTIONS
        catch (const syntax_error& yyexc)
          {
            YYCDEBUG << "Caught exception: " << yyexc.what() << '\n';
            error (yyexc);
            goto yyerrlab1;
          }
#endif // YY_EXCEPTIONS
      }
    YY_SYMBOL_PRINT ("Next token is", yyla);

    if (yyla.kind () == symbol_kind::S_YYerror)
    {
      // The scanner already issued an error message, process directly
      // to error recovery.  But do not keep the error token as
      // lookahead, it is too special and may lead us to an endless
      // loop in error recovery. */
      yyla.kind_ = symbol_kind::S_YYUNDEF;
      goto yyerrlab1;
    }

    /* If the proper action on seeing token YYLA.TYPE is to reduce or
       to detect an error, take that action.  */
    yyn += yyla.kind ();
    if (yyn < 0 || yylast_ < yyn || yycheck_[yyn] != yyla.kind ())
      {
        if (!yy_lac_establish_ (yyla.kind ()))
          goto yyerrlab;
        goto yydefault;
      }

    // Reduce or error.
    yyn = yytable_[yyn];
    if (yyn <= 0)
      {
        if (yy_table_value_is_error_ (yyn))
          goto yyerrlab;
        if (!yy_lac_establish_ (yyla.kind ()))
          goto yyerrlab;

        yyn = -yyn;
        goto yyreduce;
      }

    // Count tokens shifted since error; after three, turn off error status.
    if (yyerrstatus_)
      --yyerrstatus_;

    // Shift the lookahead token.
    yypush_ ("Shifting", state_type (yyn), YY_MOVE (yyla));
    yy_lac_discard_ ("shift");
    goto yynewstate;


  /*-----------------------------------------------------------.
  | yydefault -- do the default action for the current state.  |
  `-----------------------------------------------------------*/
  yydefault:
    yyn = yydefact_[+yystack_[0].state];
    if (yyn == 0)
      goto yyerrlab;
    goto yyreduce;


  /*-----------------------------.
  | yyreduce -- do a reduction.  |
  `-----------------------------*/
  yyreduce:
    yylen = yyr2_[yyn];
    {
      stack_symbol_type yylhs;
      yylhs.state = yy_lr_goto_state_ (yystack_[yylen].state, yyr1_[yyn]);
      /* Variants are always initialized to an empty instance of the
         correct type. The default '$$ = $1' action is NOT applied
         when using variants.  */
      switch (yyr1_[yyn])
    {
      case symbol_kind::S_section_type: // section_type
        yylhs.value.emplace< SectionType > ();
        break;

      case symbol_kind::S_string: // string
        yylhs.value.emplace< std::string > ();
        break;

      case symbol_kind::S_number: // number
        yylhs.value.emplace< uint32_t > ();
        break;

      default:
        break;
    }



      // Perform the reduction.
      YY_REDUCE_PRINT (yyn);
#if YY_EXCEPTIONS
      try
#endif // YY_EXCEPTIONS
        {
          switch (yyn)
            {
  case 4: // line: "INCLUDE" string newline
#line 70 "src/link/script.y"
                               { includeFile(std::move(yystack_[1].value.as < std::string > ())); }
#line 615 "src/link/script.cpp"
    break;

  case 5: // line: directive newline
#line 71 "src/link/script.y"
                        { incLineNo(); }
#line 621 "src/link/script.cpp"
    break;

  case 6: // line: newline
#line 72 "src/link/script.y"
              { incLineNo(); }
#line 627 "src/link/script.cpp"
    break;

  case 7: // line: error newline
#line 73 "src/link/script.y"
                    { yyerrok; incLineNo(); }
#line 633 "src/link/script.cpp"
    break;

  case 8: // directive: section_type
#line 76 "src/link/script.y"
                        { setSectionType(yystack_[0].value.as < SectionType > ()); }
#line 639 "src/link/script.cpp"
    break;

  case 9: // directive: section_type number
#line 77 "src/link/script.y"
                               { setSectionType(yystack_[1].value.as < SectionType > (), yystack_[0].value.as < uint32_t > ()); }
#line 645 "src/link/script.cpp"
    break;

  case 10: // directive: "ORG" number
#line 78 "src/link/script.y"
                        { setAddr(yystack_[0].value.as < uint32_t > ()); }
#line 651 "src/link/script.cpp"
    break;

  case 11: // directive: "ALIGN" number
#line 79 "src/link/script.y"
                          { alignTo(yystack_[0].value.as < uint32_t > (), 0); }
#line 657 "src/link/script.cpp"
    break;

  case 12: // directive: "DS" number
#line 80 "src/link/script.y"
                       { pad(yystack_[0].value.as < uint32_t > ()); }
#line 663 "src/link/script.cpp"
    break;

  case 13: // directive: string
#line 81 "src/link/script.y"
                  { placeSection(yystack_[0].value.as < std::string > ()); }
#line 669 "src/link/script.cpp"
    break;


#line 673 "src/link/script.cpp"

            default:
              break;
            }
        }
#if YY_EXCEPTIONS
      catch (const syntax_error& yyexc)
        {
          YYCDEBUG << "Caught exception: " << yyexc.what() << '\n';
          error (yyexc);
          YYERROR;
        }
#endif // YY_EXCEPTIONS
      YY_SYMBOL_PRINT ("-> $$ =", yylhs);
      yypop_ (yylen);
      yylen = 0;

      // Shift the result of the reduction.
      yypush_ (YY_NULLPTR, YY_MOVE (yylhs));
    }
    goto yynewstate;


  /*--------------------------------------.
  | yyerrlab -- here on detecting error.  |
  `--------------------------------------*/
  yyerrlab:
    // If not already recovering from an error, report this error.
    if (!yyerrstatus_)
      {
        ++yynerrs_;
        context yyctx (*this, yyla);
        std::string msg = yysyntax_error_ (yyctx);
        error (YY_MOVE (msg));
      }


    if (yyerrstatus_ == 3)
      {
        /* If just tried and failed to reuse lookahead token after an
           error, discard it.  */

        // Return failure if at end of input.
        if (yyla.kind () == symbol_kind::S_YYEOF)
          YYABORT;
        else if (!yyla.empty ())
          {
            yy_destroy_ ("Error: discarding", yyla);
            yyla.clear ();
          }
      }

    // Else will try to reuse lookahead token after shifting the error token.
    goto yyerrlab1;


  /*---------------------------------------------------.
  | yyerrorlab -- error raised explicitly by YYERROR.  |
  `---------------------------------------------------*/
  yyerrorlab:
    /* Pacify compilers when the user code never invokes YYERROR and
       the label yyerrorlab therefore never appears in user code.  */
    if (false)
      YYERROR;

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYERROR.  */
    yypop_ (yylen);
    yylen = 0;
    YY_STACK_PRINT ();
    goto yyerrlab1;


  /*-------------------------------------------------------------.
  | yyerrlab1 -- common code for both syntax error and YYERROR.  |
  `-------------------------------------------------------------*/
  yyerrlab1:
    yyerrstatus_ = 3;   // Each real token shifted decrements this.
    // Pop stack until we find a state that shifts the error token.
    for (;;)
      {
        yyn = yypact_[+yystack_[0].state];
        if (!yy_pact_value_is_default_ (yyn))
          {
            yyn += symbol_kind::S_YYerror;
            if (0 <= yyn && yyn <= yylast_
                && yycheck_[yyn] == symbol_kind::S_YYerror)
              {
                yyn = yytable_[yyn];
                if (0 < yyn)
                  break;
              }
          }

        // Pop the current state because it cannot handle the error token.
        if (yystack_.size () == 1)
          YYABORT;

        yy_destroy_ ("Error: popping", yystack_[0]);
        yypop_ ();
        YY_STACK_PRINT ();
      }
    {
      stack_symbol_type error_token;


      // Shift the error token.
      yy_lac_discard_ ("error recovery");
      error_token.state = state_type (yyn);
      yypush_ ("Shifting", YY_MOVE (error_token));
    }
    goto yynewstate;


  /*-------------------------------------.
  | yyacceptlab -- YYACCEPT comes here.  |
  `-------------------------------------*/
  yyacceptlab:
    yyresult = 0;
    goto yyreturn;


  /*-----------------------------------.
  | yyabortlab -- YYABORT comes here.  |
  `-----------------------------------*/
  yyabortlab:
    yyresult = 1;
    goto yyreturn;


  /*-----------------------------------------------------.
  | yyreturn -- parsing is finished, return the result.  |
  `-----------------------------------------------------*/
  yyreturn:
    if (!yyla.empty ())
      yy_destroy_ ("Cleanup: discarding lookahead", yyla);

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYABORT or YYACCEPT.  */
    yypop_ (yylen);
    YY_STACK_PRINT ();
    while (1 < yystack_.size ())
      {
        yy_destroy_ ("Cleanup: popping", yystack_[0]);
        yypop_ ();
      }

    return yyresult;
  }
#if YY_EXCEPTIONS
    catch (...)
      {
        YYCDEBUG << "Exception caught: cleaning lookahead and stack\n";
        // Do not try to display the values of the reclaimed symbols,
        // as their printers might throw an exception.
        if (!yyla.empty ())
          yy_destroy_ (YY_NULLPTR, yyla);

        while (1 < yystack_.size ())
          {
            yy_destroy_ (YY_NULLPTR, yystack_[0]);
            yypop_ ();
          }
        throw;
      }
#endif // YY_EXCEPTIONS
  }

  void
  parser::error (const syntax_error& yyexc)
  {
    error (yyexc.what ());
  }

  const char *
  parser::symbol_name (symbol_kind_type yysymbol)
  {
    static const char *const yy_sname[] =
    {
    "end of file", "error", "invalid token", "newline", "ORG", "INCLUDE",
  "ALIGN", "DS", "string", "number", "section_type", "$accept", "lines",
  "line", "directive", YY_NULLPTR
    };
    return yy_sname[yysymbol];
  }



  // parser::context.
  parser::context::context (const parser& yyparser, const symbol_type& yyla)
    : yyparser_ (yyparser)
    , yyla_ (yyla)
  {}

  int
  parser::context::expected_tokens (symbol_kind_type yyarg[], int yyargn) const
  {
    // Actual number of expected tokens
    int yycount = 0;

#if YYDEBUG
    // Execute LAC once. We don't care if it is successful, we
    // only do it for the sake of debugging output.
    if (!yyparser_.yy_lac_established_)
      yyparser_.yy_lac_check_ (yyla_.kind ());
#endif

    for (int yyx = 0; yyx < YYNTOKENS; ++yyx)
      {
        symbol_kind_type yysym = YY_CAST (symbol_kind_type, yyx);
        if (yysym != symbol_kind::S_YYerror
            && yysym != symbol_kind::S_YYUNDEF
            && yyparser_.yy_lac_check_ (yysym))
          {
            if (!yyarg)
              ++yycount;
            else if (yycount == yyargn)
              return 0;
            else
              yyarg[yycount++] = yysym;
          }
      }
    if (yyarg && yycount == 0 && 0 < yyargn)
      yyarg[0] = symbol_kind::S_YYEMPTY;
    return yycount;
  }




  bool
  parser::yy_lac_check_ (symbol_kind_type yytoken) const
  {
    // Logically, the yylac_stack's lifetime is confined to this function.
    // Clear it, to get rid of potential left-overs from previous call.
    yylac_stack_.clear ();
    // Reduce until we encounter a shift and thereby accept the token.
#if YYDEBUG
    YYCDEBUG << "LAC: checking lookahead " << symbol_name (yytoken) << ':';
#endif
    std::ptrdiff_t lac_top = 0;
    while (true)
      {
        state_type top_state = (yylac_stack_.empty ()
                                ? yystack_[lac_top].state
                                : yylac_stack_.back ());
        int yyrule = yypact_[+top_state];
        if (yy_pact_value_is_default_ (yyrule)
            || (yyrule += yytoken) < 0 || yylast_ < yyrule
            || yycheck_[yyrule] != yytoken)
          {
            // Use the default action.
            yyrule = yydefact_[+top_state];
            if (yyrule == 0)
              {
                YYCDEBUG << " Err\n";
                return false;
              }
          }
        else
          {
            // Use the action from yytable.
            yyrule = yytable_[yyrule];
            if (yy_table_value_is_error_ (yyrule))
              {
                YYCDEBUG << " Err\n";
                return false;
              }
            if (0 < yyrule)
              {
                YYCDEBUG << " S" << yyrule << '\n';
                return true;
              }
            yyrule = -yyrule;
          }
        // By now we know we have to simulate a reduce.
        YYCDEBUG << " R" << yyrule - 1;
        // Pop the corresponding number of values from the stack.
        {
          std::ptrdiff_t yylen = yyr2_[yyrule];
          // First pop from the LAC stack as many tokens as possible.
          std::ptrdiff_t lac_size = std::ptrdiff_t (yylac_stack_.size ());
          if (yylen < lac_size)
            {
              yylac_stack_.resize (std::size_t (lac_size - yylen));
              yylen = 0;
            }
          else if (lac_size)
            {
              yylac_stack_.clear ();
              yylen -= lac_size;
            }
          // Only afterwards look at the main stack.
          // We simulate popping elements by incrementing lac_top.
          lac_top += yylen;
        }
        // Keep top_state in sync with the updated stack.
        top_state = (yylac_stack_.empty ()
                     ? yystack_[lac_top].state
                     : yylac_stack_.back ());
        // Push the resulting state of the reduction.
        state_type state = yy_lr_goto_state_ (top_state, yyr1_[yyrule]);
        YYCDEBUG << " G" << int (state);
        yylac_stack_.push_back (state);
      }
  }

  // Establish the initial context if no initial context currently exists.
  bool
  parser::yy_lac_establish_ (symbol_kind_type yytoken)
  {
    /* Establish the initial context for the current lookahead if no initial
       context is currently established.

       We define a context as a snapshot of the parser stacks.  We define
       the initial context for a lookahead as the context in which the
       parser initially examines that lookahead in order to select a
       syntactic action.  Thus, if the lookahead eventually proves
       syntactically unacceptable (possibly in a later context reached via a
       series of reductions), the initial context can be used to determine
       the exact set of tokens that would be syntactically acceptable in the
       lookahead's place.  Moreover, it is the context after which any
       further semantic actions would be erroneous because they would be
       determined by a syntactically unacceptable token.

       yy_lac_establish_ should be invoked when a reduction is about to be
       performed in an inconsistent state (which, for the purposes of LAC,
       includes consistent states that don't know they're consistent because
       their default reductions have been disabled).

       For parse.lac=full, the implementation of yy_lac_establish_ is as
       follows.  If no initial context is currently established for the
       current lookahead, then check if that lookahead can eventually be
       shifted if syntactic actions continue from the current context.  */
    if (yy_lac_established_)
      return true;
    else
      {
#if YYDEBUG
        YYCDEBUG << "LAC: initial context established for "
                 << symbol_name (yytoken) << '\n';
#endif
        yy_lac_established_ = true;
        return yy_lac_check_ (yytoken);
      }
  }

  // Discard any previous initial lookahead context.
  void
  parser::yy_lac_discard_ (const char* event)
  {
   /* Discard any previous initial lookahead context because of Event,
      which may be a lookahead change or an invalidation of the currently
      established initial context for the current lookahead.

      The most common example of a lookahead change is a shift.  An example
      of both cases is syntax error recovery.  That is, a syntax error
      occurs when the lookahead is syntactically erroneous for the
      currently established initial context, so error recovery manipulates
      the parser stacks to try to find a new initial context in which the
      current lookahead is syntactically acceptable.  If it fails to find
      such a context, it discards the lookahead.  */
    if (yy_lac_established_)
      {
        YYCDEBUG << "LAC: initial context discarded due to "
                 << event << '\n';
        yy_lac_established_ = false;
      }
  }


  int
  parser::yy_syntax_error_arguments_ (const context& yyctx,
                                                 symbol_kind_type yyarg[], int yyargn) const
  {
    /* There are many possibilities here to consider:
       - If this state is a consistent state with a default action, then
         the only way this function was invoked is if the default action
         is an error action.  In that case, don't check for expected
         tokens because there are none.
       - The only way there can be no lookahead present (in yyla) is
         if this state is a consistent state with a default action.
         Thus, detecting the absence of a lookahead is sufficient to
         determine that there is no unexpected or expected token to
         report.  In that case, just report a simple "syntax error".
       - Don't assume there isn't a lookahead just because this state is
         a consistent state with a default action.  There might have
         been a previous inconsistent state, consistent state with a
         non-default action, or user semantic action that manipulated
         yyla.  (However, yyla is currently not documented for users.)
         In the first two cases, it might appear that the current syntax
         error should have been detected in the previous state when
         yy_lac_check was invoked.  However, at that time, there might
         have been a different syntax error that discarded a different
         initial context during error recovery, leaving behind the
         current lookahead.
    */

    if (!yyctx.lookahead ().empty ())
      {
        if (yyarg)
          yyarg[0] = yyctx.token ();
        int yyn = yyctx.expected_tokens (yyarg ? yyarg + 1 : yyarg, yyargn - 1);
        return yyn + 1;
      }
    return 0;
  }

  // Generate an error message.
  std::string
  parser::yysyntax_error_ (const context& yyctx) const
  {
    // Its maximum.
    enum { YYARGS_MAX = 5 };
    // Arguments of yyformat.
    symbol_kind_type yyarg[YYARGS_MAX];
    int yycount = yy_syntax_error_arguments_ (yyctx, yyarg, YYARGS_MAX);

    char const* yyformat = YY_NULLPTR;
    switch (yycount)
      {
#define YYCASE_(N, S)                         \
        case N:                               \
          yyformat = S;                       \
        break
      default: // Avoid compiler warnings.
        YYCASE_ (0, YY_("syntax error"));
        YYCASE_ (1, YY_("syntax error, unexpected %s"));
        YYCASE_ (2, YY_("syntax error, unexpected %s, expecting %s"));
        YYCASE_ (3, YY_("syntax error, unexpected %s, expecting %s or %s"));
        YYCASE_ (4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
        YYCASE_ (5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
      }

    std::string yyres;
    // Argument number.
    std::ptrdiff_t yyi = 0;
    for (char const* yyp = yyformat; *yyp; ++yyp)
      if (yyp[0] == '%' && yyp[1] == 's' && yyi < yycount)
        {
          yyres += symbol_name (yyarg[yyi++]);
          ++yyp;
        }
      else
        yyres += *yyp;
    return yyres;
  }


  const signed char parser::yypact_ninf_ = -2;

  const signed char parser::yytable_ninf_ = -3;

  const signed char
  parser::yypact_[] =
  {
       0,    -1,    -2,     2,     1,     3,     4,    -2,     5,    15,
       0,    13,    -2,    -2,    14,    -2,    -2,    -2,    -2,    -2,
      -2,    -2
  };

  const signed char
  parser::yydefact_[] =
  {
       0,     0,     6,     0,     0,     0,     0,    13,     8,     0,
       0,     0,     7,    10,     0,    11,    12,     9,     1,     3,
       5,     4
  };

  const signed char
  parser::yypgoto_[] =
  {
      -2,     8,    -2,    -2
  };

  const signed char
  parser::yydefgoto_[] =
  {
       0,     9,    10,    11
  };

  const signed char
  parser::yytable_[] =
  {
      -2,     1,    12,     2,     3,     4,     5,     6,     7,    14,
       8,    13,    15,    16,    17,    18,    20,    21,    19
  };

  const signed char
  parser::yycheck_[] =
  {
       0,     1,     3,     3,     4,     5,     6,     7,     8,     8,
      10,     9,     9,     9,     9,     0,     3,     3,    10
  };

  const signed char
  parser::yystos_[] =
  {
       0,     1,     3,     4,     5,     6,     7,     8,    10,    12,
      13,    14,     3,     9,     8,     9,     9,     9,     0,    12,
       3,     3
  };

  const signed char
  parser::yyr1_[] =
  {
       0,    11,    12,    12,    13,    13,    13,    13,    14,    14,
      14,    14,    14,    14
  };

  const signed char
  parser::yyr2_[] =
  {
       0,     2,     0,     2,     3,     2,     1,     2,     1,     2,
       2,     2,     2,     1
  };




#if YYDEBUG
  const signed char
  parser::yyrline_[] =
  {
       0,    66,    66,    67,    70,    71,    72,    73,    76,    77,
      78,    79,    80,    81
  };

  void
  parser::yy_stack_print_ () const
  {
    *yycdebug_ << "Stack now";
    for (stack_type::const_iterator
           i = yystack_.begin (),
           i_end = yystack_.end ();
         i != i_end; ++i)
      *yycdebug_ << ' ' << int (i->state);
    *yycdebug_ << '\n';
  }

  void
  parser::yy_reduce_print_ (int yyrule) const
  {
    int yylno = yyrline_[yyrule];
    int yynrhs = yyr2_[yyrule];
    // Print the symbols being reduced, and their result.
    *yycdebug_ << "Reducing stack by rule " << yyrule - 1
               << " (line " << yylno << "):\n";
    // The symbols being reduced.
    for (int yyi = 0; yyi < yynrhs; yyi++)
      YY_SYMBOL_PRINT ("   $" << yyi + 1 << " =",
                       yystack_[(yynrhs) - (yyi + 1)]);
  }
#endif // YYDEBUG


} // yy
#line 1232 "src/link/script.cpp"

#line 84 "src/link/script.y"


#define scriptError(context, fmt, ...) ::error(NULL, 0, "%s(%" PRIu32 "): " fmt, context.path.c_str(), context.lineNo __VA_OPT__(,) __VA_ARGS__)

// Lexer.

struct LexerStackEntry {
	std::filebuf file;
	std::string path;
	uint32_t lineNo;

	using int_type = decltype(file)::int_type;
	static constexpr int_type eof = decltype(file)::traits_type::eof();

	explicit LexerStackEntry(std::string &&path_) : file(), path(path_), lineNo(0) {}
};
static std::vector<LexerStackEntry> lexerStack;

void yy::parser::error(std::string const &msg) {
	auto const &script = lexerStack.back();
	scriptError(script, "%s", msg.c_str());
}

static void includeFile(std::string &&path) {
	auto &newEntry = lexerStack.emplace_back(path);
	auto &prevEntry = lexerStack[lexerStack.size() - 2];

	if (!newEntry.file.open(newEntry.path, std::ios_base::in)) {
		scriptError(prevEntry, "Could not open included linker script \"%s\"", newEntry.path.c_str());
		++prevEntry.lineNo; // Do this after reporting the error, but before modifying the stack!
		lexerStack.pop_back();
	} else {
		// The lexer will use the new entry to lex the next token.
		++prevEntry.lineNo;
	}
}

static void incLineNo(void) {
	++lexerStack.back().lineNo;
}

static bool isWhiteSpace(LexerStackEntry::int_type c) {
	return c == ' ' || c == '\t';
}

static bool isNewline(LexerStackEntry::int_type c) {
	return c == '\r' || c == '\n';
}

static bool isIdentChar(LexerStackEntry::int_type c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

yy::parser::symbol_type yylex(void) {
	do {
		auto &context = lexerStack.back();
		auto c = context.file.sbumpc();

		// First, skip leading whitespace.
		while (isWhiteSpace(c)) {
			c = context.file.sbumpc();
		}
		// Then, skip a comment if applicable.
		if (c == ';') {
			while (!isNewline(c)) {
				c = context.file.sbumpc();
			}
		}

		// Alright, what token should we return?
		if (c == LexerStackEntry::eof) {
			// Basically yywrap().
			lexerStack.pop_back();
			if (!lexerStack.empty()) {
				continue;
			}
			return yy::parser::make_YYEOF();
		} else if (isNewline(c)) {
			// Handle CRLF.
			if (c == '\r' && context.file.sgetc() == '\n') {
				context.file.sbumpc();
			}
			return yy::parser::make_newline();
		} else if (c == '"') {
			std::string str;

			for (c = context.file.sgetc(); c != '"'; c = context.file.sgetc()) {
				if (c == LexerStackEntry::eof || isNewline(c)) {
					scriptError(context, "Unterminated string");
					break;
				}
				context.file.sbumpc();
				str.push_back(c);
			}

			return yy::parser::make_string(std::move(str));
		} else if (isIdentChar(c)) {
			std::string ident;
			auto strCaseCmp = [](char cmp, char ref) {
				// `locale::classic()` yields the "C" locale.
				assert(std::use_facet<std::ctype<char>>(std::locale::classic()).is(std::ctype_base::upper, ref));
				return std::use_facet<std::ctype<char>>(std::locale::classic()).toupper(cmp) == ref;
			};

			ident.push_back(c);
			for (c = context.file.sgetc(); isIdentChar(c); c = context.file.snextc()) {
				ident.push_back(c);
			}

			for (SectionType type : EnumSeq(SECTTYPE_INVALID)) {
				if (std::ranges::equal(ident, sectionTypeInfo[type].name, strCaseCmp)) {
					return yy::parser::make_section_type(type);
				}
			}

			for (Keyword const &keyword : keywords) {
				if (std::ranges::equal(ident, keyword.name, strCaseCmp)) {
					return keyword.tokenGen();
				}
			}

			scriptError(context, "Unknown keyword \"%s\"", ident.c_str());
			continue; // Try lexing another token.
		} else if (c == '$') {
			abort(); // TODO: hex number
		} else if (c == '%') {
			abort(); // TOOD: bin number
		} else if (c >= '0' && c <= '9') {
			abort(); // TODO: dec number
		} else {
			abort(); // TODO: UTF-8 decoding
			scriptError(context, "Unexpected character '%s'", printChar(c));
			// Keep reading characters until the EOL, to avoid reporting too many errors.
			abort();
		}
	} while (0); // This will generate a warning if any codepath forgets to return a token or loop.
}

// Semantic actions.

static SectionType activeType;
static uint32_t activeBankIdx; // This is the index into the array, not
static std::array<std::vector<uint16_t>, SECTTYPE_INVALID> curAddr;

static void setActiveTypeAndIdx(SectionType type, uint32_t idx) {
	activeType = type;
	activeBankIdx = idx;
	if (curAddr[activeType].size() <= activeBankIdx) {
		curAddr[activeType].resize(activeBankIdx + 1, sectionTypeInfo[type].startAddr);
	}
}

static void setSectionType(SectionType type) {
	auto const &context = lexerStack.back();

	if (nbbanks(type) != 1) {
		scriptError(context, "A bank number must be specified for %s", sectionTypeInfo[type].name.c_str());
		// Keep going with a default value for the bank index.
	}

	setActiveTypeAndIdx(type, 0); // There is only a single bank anyway, so just set the index to 0.
}

static void setSectionType(SectionType type, uint32_t bank) {
	auto const &context = lexerStack.back();
	auto const &typeInfo = sectionTypeInfo[type];

	if (bank < typeInfo.firstBank) {
		scriptError(context, "%s bank %" PRIu32 " doesn't exist, the minimum is %" PRIu32,
		            typeInfo.name.c_str(), bank, typeInfo.firstBank);
		bank = typeInfo.firstBank;
	} else if (bank > typeInfo.lastBank) {
		scriptError(context, "%s bank %" PRIu32 " doesn't exist, the maximum is %" PRIu32,
		            typeInfo.name.c_str(), bank, typeInfo.lastBank);
	}

	setActiveTypeAndIdx(type, bank - typeInfo.firstBank);
}

static void setAddr(uint32_t addr) {
	auto const &context = lexerStack.back();
	auto &pc = curAddr[activeType][activeBankIdx];
	auto const &typeInfo = sectionTypeInfo[activeType];

	if (addr < pc) {
		scriptError(context, "ORG cannot be used to go backwards (from $%04x to $%04x)", pc, addr);
	} else if (addr > endaddr(activeType)) { // Allow "one past the end" sections.
		scriptError(context, "Cannot go to $%04" PRIx16 ": %s ends at $%04" PRIx16 "",
		            addr, typeInfo.name.c_str(), endaddr(activeType));
		pc = endaddr(activeType);
	} else {
		pc = addr;
	}
}

static void alignTo(uint32_t alignment, uint32_t alignOfs) {
	auto const &context = lexerStack.back();
	auto const &typeInfo = sectionTypeInfo[activeType];
	auto &pc = curAddr[activeType][activeBankIdx];

	// TODO: maybe warn if truncating?
	alignOfs %= 1 << alignment;

	assert(pc >= typeInfo.startAddr);
	uint16_t length = alignment < 16 ? (uint16_t)(alignOfs - pc) % (1u << alignment)
	                                 : alignOfs - pc; // Let it wrap around, this'll trip the check.
	if (uint16_t offset = pc - typeInfo.startAddr; length > typeInfo.size - offset) {
		scriptError(context, "Cannot align: the next suitable address after $%04" PRIx16 " is $%04" PRIx16 ", past $%04" PRIx16,
		            pc, (uint16_t)(pc + length), endaddr(activeType) + 1);
	} else {
		pc += length;
	}
}

static void pad(uint32_t length) {
	auto const &context = lexerStack.back();
	auto const &typeInfo = sectionTypeInfo[activeType];
	auto &pc = curAddr[activeType][activeBankIdx];

	assert(pc >= typeInfo.startAddr);
	if (uint16_t offset = pc - typeInfo.startAddr; length > typeInfo.size - offset) {
		scriptError(context, "Cannot pad by %u bytes: only %u bytes to $%04" PRIx16,
		            length, typeInfo.size - offset, endaddr(activeType) + 1);
	} else {
		pc += length;
	}
}

static void placeSection(std::string const &name) {
	auto const &context = lexerStack.back();

	// A type *must* be active.
	if (activeType == SECTTYPE_INVALID) {
		scriptError(context, "No memory region has been specified to place section \"%s\" in", name.c_str());
		return;
	}

	auto *section = sect_GetSection(name.c_str());
	if (!section) {
		scriptError(context, "Unknown section \"%s\"", name.c_str());
		return;
	}

	assert(section->offset == 0);
	// Check that the linker script doesn't contradict what the code says.
	if (section->type == SECTTYPE_INVALID) {
		// SDCC areas don't have a type assigned yet, so the linker script is used to give them one.
		for (Section *fragment = section; fragment; fragment = fragment->nextu) {
			fragment->type = activeType;
		}
	} else if (section->type != activeType) {
		scriptError(context, "\"%s\" is specified to be a %s section, but it is already a %s section",
		            name.c_str(), sectionTypeInfo[activeType].name.c_str(), sectionTypeInfo[section->type].name.c_str());
	}

	uint32_t bank = activeBankIdx + sectionTypeInfo[activeType].firstBank;
	abort(); // TODO: the rest.
}

// TODO: external API

void script_ProcessScript(char const *path) {
	activeType = SECTTYPE_INVALID;

	lexerStack.clear();
	auto &newEntry = lexerStack.emplace_back(std::string(path));

	if (!newEntry.file.open(newEntry.path, std::ios_base::in)) {
		error(NULL, 0, "Could not open linker script \"%s\"", newEntry.path.c_str());
		lexerStack.clear();
	} else {
		yy::parser linkerScriptParser;
		linkerScriptParser(); // TODO: check the return value
	}
}
