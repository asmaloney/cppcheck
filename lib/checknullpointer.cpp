/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2012 Daniel Marjamäki and Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


//---------------------------------------------------------------------------
#include "checknullpointer.h"
#include "executionpath.h"
#include "mathlib.h"
#include "symboldatabase.h"
#include <cctype>
//---------------------------------------------------------------------------

// Register this check class (by creating a static instance of it)
namespace {
    CheckNullPointer instance;
}

//---------------------------------------------------------------------------


/** Is string uppercase? */
bool CheckNullPointer::isUpper(const std::string &str)
{
    for (unsigned int i = 0; i < str.length(); ++i) {
        if (std::islower(str[i]))
            return false;
    }
    return true;
}

/**
 * @brief parse a function call and extract information about variable usage
 * @param tok first token
 * @param var variables that the function read / write.
 * @param value 0 => invalid with null pointers as parameter.
 *              1-.. => invalid with uninitialized data.
 */
void CheckNullPointer::parseFunctionCall(const Token &tok, std::list<const Token *> &var, unsigned char value)
{
    // standard functions that dereference first parameter..
    static std::set<std::string> functionNames1_all;
    static std::set<std::string> functionNames1_nullptr;
    static std::set<std::string> functionNames1_uninit;
    if (functionNames1_all.empty()) {
        // cstdlib
        functionNames1_all.insert("atoi");
        functionNames1_all.insert("atof");
        functionNames1_all.insert("atol");
        functionNames1_all.insert("qsort");
        functionNames1_all.insert("strtod");
        functionNames1_all.insert("strtol");
        functionNames1_all.insert("strtoul");
        // cstring
        functionNames1_all.insert("memchr");
        functionNames1_all.insert("memcmp");
        functionNames1_all.insert("strcat");
        functionNames1_all.insert("strncat");
        functionNames1_all.insert("strcoll");
        functionNames1_all.insert("strchr");
        functionNames1_all.insert("strrchr");
        functionNames1_all.insert("strcmp");
        functionNames1_all.insert("strncmp");
        functionNames1_all.insert("strcspn");
        functionNames1_all.insert("strdup");
        functionNames1_all.insert("strndup");
        functionNames1_all.insert("strpbrk");
        functionNames1_all.insert("strlen");
        functionNames1_all.insert("strspn");
        functionNames1_all.insert("strstr");
        // cstdio
        functionNames1_all.insert("fclose");
        functionNames1_all.insert("feof");
        functionNames1_all.insert("fwrite");
        functionNames1_all.insert("fseek");
        functionNames1_all.insert("ftell");
        functionNames1_all.insert("fputs");
        functionNames1_all.insert("ferror");
        functionNames1_all.insert("fgetc");
        functionNames1_all.insert("fgetpos");
        functionNames1_all.insert("fsetpos");
        functionNames1_all.insert("freopen");
        functionNames1_all.insert("fscanf");
        functionNames1_all.insert("fprintf");
        functionNames1_all.insert("fopen");
        functionNames1_all.insert("rewind");
        functionNames1_all.insert("printf");
        functionNames1_all.insert("scanf");
        functionNames1_all.insert("fscanf");
        functionNames1_all.insert("sscanf");
        functionNames1_all.insert("setbuf");
        functionNames1_all.insert("setvbuf");
        functionNames1_all.insert("rename");
        functionNames1_all.insert("remove");
        functionNames1_all.insert("puts");
        functionNames1_all.insert("getc");
        functionNames1_all.insert("clearerr");
        // ctime
        functionNames1_all.insert("asctime");
        functionNames1_all.insert("ctime");
        functionNames1_all.insert("mktime");

        functionNames1_nullptr.insert("itoa");
        functionNames1_nullptr.insert("mbstowcs");
        functionNames1_nullptr.insert("wcstombs");
        functionNames1_nullptr.insert("memcpy");
        functionNames1_nullptr.insert("memmove");
        functionNames1_nullptr.insert("memset");
        functionNames1_nullptr.insert("strcpy");
        functionNames1_nullptr.insert("sprintf");
        functionNames1_nullptr.insert("vsprintf");
        functionNames1_nullptr.insert("vprintf");
        functionNames1_nullptr.insert("fprintf");
        functionNames1_nullptr.insert("vfprintf");
        functionNames1_nullptr.insert("tmpnam");
        functionNames1_nullptr.insert("fread");
        functionNames1_nullptr.insert("gets");
        functionNames1_nullptr.insert("gmtime");
        functionNames1_nullptr.insert("localtime");
        functionNames1_nullptr.insert("strftime");

        functionNames1_uninit.insert("perror");
        functionNames1_uninit.insert("fflush");
    }

    // standard functions that dereference second parameter..
    static std::set<std::string> functionNames2_all;
    static std::set<std::string> functionNames2_nullptr;
    if (functionNames2_all.empty()) {
        functionNames2_all.insert("mbstowcs");
        functionNames2_all.insert("wcstombs");
        functionNames2_all.insert("memcmp");
        functionNames2_all.insert("memcpy");
        functionNames2_all.insert("memmove");
        functionNames2_all.insert("strcat");
        functionNames2_all.insert("strncat");
        functionNames2_all.insert("strcmp");
        functionNames2_all.insert("strncmp");
        functionNames2_all.insert("strcoll");
        functionNames2_all.insert("strcpy");
        functionNames2_all.insert("strcspn");
        functionNames2_all.insert("strncpy");
        functionNames2_all.insert("strpbrk");
        functionNames2_all.insert("strspn");
        functionNames2_all.insert("strstr");
        functionNames2_all.insert("strxfrm");
        functionNames2_all.insert("sprintf");
        functionNames2_all.insert("fprintf");
        functionNames2_all.insert("fscanf");
        functionNames2_all.insert("sscanf");
        functionNames2_all.insert("fputs");
        functionNames2_all.insert("fputc");
        functionNames2_all.insert("ungetc");
        functionNames2_all.insert("rename");
        functionNames2_all.insert("putc");
        functionNames2_all.insert("freopen");

        functionNames2_nullptr.insert("frexp");
        functionNames2_nullptr.insert("modf");
        functionNames2_nullptr.insert("fgetpos");
    }

    if (Token::Match(&tok, "%var% ( )") || !tok.tokAt(2))
        return;

    const Token* firstParam = tok.tokAt(2);
    const Token* secondParam = firstParam->nextArgument();

    // 1st parameter..
    if ((Token::Match(firstParam, "%var% ,|)") && firstParam->varId() > 0) ||
        (value == 0 && Token::Match(firstParam, "0 ,|)"))) {
        if (functionNames1_all.find(tok.str()) != functionNames1_all.end())
            var.push_back(firstParam);
        else if (value == 0 && functionNames1_nullptr.find(tok.str()) != functionNames1_nullptr.end())
            var.push_back(firstParam);
        else if (value != 0 && functionNames1_uninit.find(tok.str()) != functionNames1_uninit.end())
            var.push_back(firstParam);
        else if (value == 0 && Token::Match(&tok, "snprintf|vsnprintf|fnprintf|vfnprintf") && secondParam && secondParam->str() != "0") // Only if length (second parameter) is not zero
            var.push_back(firstParam);
    }

    // 2nd parameter..
    if (secondParam && ((value == 0 && secondParam->str() == "0") || (Token::Match(secondParam, "%var%") && secondParam->varId() > 0))) {
        if (functionNames2_all.find(tok.str()) != functionNames2_all.end())
            var.push_back(secondParam);
        else if (value == 0 && functionNames2_nullptr.find(tok.str()) != functionNames2_nullptr.end())
            var.push_back(secondParam);
    }

    if (Token::Match(&tok, "printf|sprintf|snprintf|fprintf|fnprintf|scanf|sscanf|fscanf")) {
        const Token* argListTok = 0; // Points to first va_list argument
        std::string formatString;
        bool scan = Token::Match(&tok, "scanf|sscanf|fscanf");

        if (Token::Match(&tok, "printf|scanf ( %str%")) {
            formatString = firstParam->strValue();
            argListTok = secondParam;
        } else if (Token::Match(&tok, "sprintf|fprintf|sscanf|fscanf")) {
            const Token* formatStringTok = secondParam; // Find second parameter (format string)
            if (formatStringTok && formatStringTok->type() == Token::eString) {
                argListTok = formatStringTok->nextArgument(); // Find third parameter (first argument of va_args)
                formatString = formatStringTok->strValue();
            }
        } else if (Token::Match(&tok, "snprintf|fnprintf") && secondParam) {
            const Token* formatStringTok = secondParam->nextArgument(); // Find third parameter (format string)
            if (formatStringTok && formatStringTok->type() == Token::eString) {
                argListTok = formatStringTok->nextArgument(); // Find fourth parameter (first argument of va_args)
                formatString = formatStringTok->strValue();
            }
        }

        if (argListTok) {
            bool percent = false;
            for (std::string::iterator i = formatString.begin(); i != formatString.end(); ++i) {
                if (*i == '%') {
                    percent = !percent;
                } else if (percent) {
                    percent = false;

                    bool _continue = false;
                    while (!std::isalpha(*i)) {
                        if (*i == '*') {
                            if (scan)
                                _continue = true;
                            else
                                argListTok = argListTok->nextArgument();
                        }
                        ++i;
                        if (!argListTok || i == formatString.end())
                            return;
                    }
                    if (_continue)
                        continue;

                    if ((*i == 'n' || *i == 's' || scan) && (!scan || value == 0)) {
                        if ((value == 0 && argListTok->str() == "0") || (argListTok->varId() > 0)) {
                            var.push_back(argListTok);
                        }
                    }

                    if (*i != 'm') // %m is a non-standard glibc extension that requires no parameter
                        argListTok = argListTok->nextArgument(); // Find next argument
                    if (!argListTok)
                        break;
                }
            }
        }
    }
}


/**
 * Is there a pointer dereference? Everything that should result in
 * a nullpointer dereference error message will result in a true
 * return value. If it's unknown if the pointer is dereferenced false
 * is returned.
 * @param tok token for the pointer
 * @param unknown it is not known if there is a pointer dereference (could be reported as a debug message)
 * @return true => there is a dereference
 */
bool CheckNullPointer::isPointerDeRef(const Token *tok, bool &unknown, const SymbolDatabase* symbolDatabase)
{
    const bool inconclusive = unknown;

    unknown = false;

    // Dereferencing pointer..
    if (tok->strAt(-1) == "*" && (Token::Match(tok->tokAt(-2), "return|throw|;|{|}|:|[|(|,") || tok->tokAt(-2)->isOp() || tok->tokAt(-2)->isAssignmentOp()) && !Token::Match(tok->tokAt(-3), "sizeof|decltype"))
        return true;

    // read/write member variable
    if (!Token::simpleMatch(tok->tokAt(-2), "& (") && !Token::Match(tok->tokAt(-2), "sizeof|decltype (") && tok->strAt(-1) != "&"  && tok->strAt(-1) != "&&" && Token::Match(tok->next(), ". %var%")) {
        if (tok->strAt(3) != "(")
            return true;
        unknown = true;
        return false;
    }

    if (Token::Match(tok, "%var% [") && (tok->previous()->str() != "&" || Token::Match(tok->next()->link()->next(), "[.(]")))
        return true;

    if (Token::Match(tok, "%var% ("))
        return true;

    if (Token::Match(tok, "%var% = %var% .") &&
        tok->varId() > 0 &&
        tok->varId() == tok->tokAt(2)->varId())
        return true;

    // std::string dereferences nullpointers
    if (Token::Match(tok->tokAt(-4), "std :: string ( %var% )"))
        return true;
    if (Token::Match(tok->tokAt(-2), "%var% ( %var% )")) {
        const Variable* var = symbolDatabase->getVariableFromVarId(tok->tokAt(-2)->varId());
        if (var && !var->isPointer() && !var->isArray() && Token::Match(var->typeStartToken(), "const| std :: string !!::"))
            return true;
    }

    // streams dereference nullpointers
    if (Token::Match(tok->previous(), "<<|>> %var%")) {
        const Variable* var = symbolDatabase->getVariableFromVarId(tok->varId());
        if (var && Token::Match(var->typeStartToken(), "const| char const| *")) { // Only outputing or reading to char* can cause problems
            const Token* tok2 = tok->previous(); // Find start of statement
            for (; tok2; tok2 = tok2->previous()) {
                if (Token::Match(tok2->previous(), ";|{|}|:"))
                    break;
            }
            if (Token::Match(tok2, "std :: cout|cin|cerr"))
                return true;
            if (tok2 && tok2->varId() != 0) {
                const Variable* var2 = symbolDatabase->getVariableFromVarId(tok2->varId());
                if (var2 && Token::Match(var2->typeStartToken(), "const| std :: istream|ifstream|istringstream|ostream|ofstream|ostringstream|stringstream|fstream|iostream"))
                    return true;
            }
        }
    }

    unsigned int ovarid = 0;
    if (Token::Match(tok, "%var% ==|!= %var%"))
        ovarid = tok->tokAt(2)->varId();
    else if (Token::Match(tok->tokAt(-2), "%var% ==|!= %var%"))
        ovarid = tok->tokAt(-2)->varId();
    else if (Token::Match(tok->tokAt(-2), "%var% =|+=|+ %var% )|]|,|;|+"))
        ovarid = tok->tokAt(-2)->varId();
    if (ovarid) {
        const Variable* var = symbolDatabase->getVariableFromVarId(ovarid);
        if (var && !var->isPointer() && !var->isArray() && Token::Match(var->typeStartToken(), "const| std :: string !!::"))
            return true;
    }

    // Check if it's NOT a pointer dereference.
    // This is most useful in inconclusive checking
    if (inconclusive) {
        // Not a dereference..
        if (Token::Match(tok, "%var% ="))
            return false;

        // OK to delete a null
        if (Token::Match(tok->previous(), "delete %var%") || Token::Match(tok->tokAt(-3), "delete [ ] %var%"))
            return false;

        // OK to check if pointer is null
        // OK to take address of pointer
        if (Token::Match(tok->previous(), "!|& %var%"))
            return false;

        // OK to pass pointer to function
        if (Token::Match(tok->previous(), "[(,] %var% [,)]"))
            return false;

        // Compare pointer
        if (Token::Match(tok->previous(), "(|&&|%oror%|==|!= %var%"))
            return false;
        if (Token::Match(tok, "%var% &&|%oror%|==|!=|)"))
            return false;

        // Taking address
        if (Token::Match(tok->previous(), "return|= %var% ;"))
            return false;

        // (void)var
        if (Token::Match(tok->previous(), "[{;}] %var% ;"))
            return false;

        // unknown if it's a dereference
        unknown = true;
    }

    // assume that it's not a dereference (no false positives)
    return false;
}


// check if function can assign pointer
bool CheckNullPointer::CanFunctionAssignPointer(const Token *functiontoken, unsigned int varid) const
{
    if (Token::Match(functiontoken, "if|while|for|switch|sizeof|catch"))
        return false;

    int argumentNumber = 0;
    for (const Token *arg = functiontoken->tokAt(2); arg; arg = arg->nextArgument()) {
        ++argumentNumber;
        if (Token::Match(arg, "%varid% [,)]", varid)) {
            const Token *ftok = _tokenizer->getFunctionTokenByName(functiontoken->str().c_str());
            if (!Token::Match(ftok, "%type% (")) {
                // assume that the function might assign the pointer
                return true;
            }

            ftok = ftok->tokAt(2);
            while (ftok && argumentNumber > 1) {
                ftok = ftok->nextArgument();
                --argumentNumber;
            }

            // check if it's a pointer parameter..
            while (ftok && ftok->isName())
                ftok = ftok->next();
            if (Token::Match(ftok, "* *| const| %var% [,)]"))
                // parameter is passed by value
                return false;
        }
    }

    // pointer is not passed
    return false;
}



void CheckNullPointer::nullPointerAfterLoop()
{
    const SymbolDatabase* const symbolDatabase = _tokenizer->getSymbolDatabase();

    // Locate insufficient null-pointer handling after loop
    for (std::list<Scope>::const_iterator i = symbolDatabase->scopeList.begin(); i != symbolDatabase->scopeList.end(); ++i) {
        // only interested in while ( %var% )
        // TODO: Aren't there false negatives. Shouldn't other loops be handled such as:
        //       - while ( ! %var% )
        const Token* const tok = i->classDef;
        if (i->type != Scope::eWhile || !Token::Match(tok, "while ( %var% )|&&"))
            continue;

        // Get variable id for the loop variable
        const Variable* var(symbolDatabase->getVariableFromVarId(tok->tokAt(2)->varId()));

        // Is variable a pointer?
        if (!var || !var->isPointer())
            continue;

        // Get variable name for the loop variable
        const std::string varname(tok->strAt(2));

        // Locate the end of the while loop body..
        const Token *tok2 = tok->next()->link()->next()->link();

        // Is this checking inconclusive?
        bool inconclusive = false;

        if (!tok2)
            continue;
        // Check if the variable is dereferenced after the while loop
        while (NULL != (tok2 = tok2->next())) {
            // inner and outer scopes
            if (tok2->str() == "{" || tok2->str() == "}") {
                // Not inconclusive: bail out
                if (!_settings->inconclusive)
                    break;

                inconclusive = true;

                if (tok2->str() == "}") {
                    // "}" => leaving function? then break.
                    const Token *tok3 = tok2->link()->previous();
                    if (!tok3 || !Token::Match(tok3, "[);]"))
                        break;
                    if (tok3->str() == ")" && !Token::Match(tok3->link()->previous(), "if|for|while"))
                        break;
                }
            }

            // Stop checking if "break" is found
            else if (tok2->str() == "break")
                break;

            // loop variable is found..
            else if (tok2->varId() == var->varId()) {
                // dummy variable.. is it unknown if pointer is dereferenced or not?
                bool unknown = _settings->inconclusive;

                // Is the loop variable dereferenced?
                if (CheckNullPointer::isPointerDeRef(tok2, unknown, symbolDatabase)) {
                    nullPointerError(tok2, varname, tok->linenr(), inconclusive);
                }

                else if (unknown && _settings->inconclusive) {
                    nullPointerError(tok2, varname, tok->linenr(), true);
                }

                break;
            }
        }
    }
}

void CheckNullPointer::nullPointerLinkedList()
{
    const SymbolDatabase* const symbolDatabase = _tokenizer->getSymbolDatabase();

    // looping through items in a linked list in a inner loop.
    // Here is an example:
    //    for (const Token *tok = tokens; tok; tok = tok->next) {
    //        if (tok->str() == "hello")
    //            tok = tok->next;   // <- tok might become a null pointer!
    //    }
    for (std::list<Scope>::const_iterator i = symbolDatabase->scopeList.begin(); i != symbolDatabase->scopeList.end(); ++i) {
        const Token* const tok1 = i->classDef;
        // search for a "for" scope..
        if (i->type != Scope::eFor || !tok1)
            continue;

        // is there any dereferencing occurring in the for statement
        const Token* end2 = tok1->linkAt(1);
        for (const Token *tok2 = tok1->tokAt(2); tok2 != end2; tok2 = tok2->next()) {
            // Dereferencing a variable inside the "for" parentheses..
            if (Token::Match(tok2, "%var% . %var%")) {
                // Variable id for dereferenced variable
                const unsigned int varid(tok2->varId());
                if (varid == 0)
                    continue;

                // Is this variable a pointer?
                const Variable* var = symbolDatabase->getVariableFromVarId(varid);
                if (!var || !var->isPointer())
                    continue;

                if (Token::Match(tok2->tokAt(-2), "%varid% ?", varid))
                    continue;

                // Check usage of dereferenced variable in the loop..
                for (std::list<Scope*>::const_iterator j = i->nestedList.begin(); j != i->nestedList.end(); ++j) {
                    Scope* scope = *j;
                    if (scope->type != Scope::eWhile)
                        continue;

                    // TODO: are there false negatives for "while ( %varid% ||"
                    if (Token::Match(scope->classDef->next(), "( %varid% &&|)", varid)) {
                        // Make sure there is a "break" or "return" inside the loop.
                        // Without the "break" a null pointer could be dereferenced in the
                        // for statement.
                        // indentlevel4 is a counter for { and }. When scanning the code with tok4
                        unsigned int indentlevel4 = 1;
                        for (const Token *tok4 = scope->classStart; tok4; tok4 = tok4->next()) {
                            if (tok4->str() == "{")
                                ++indentlevel4;
                            else if (tok4->str() == "}") {
                                if (indentlevel4 <= 1) {
                                    nullPointerError(tok1, var->name(), scope->classDef->linenr());
                                    break;
                                }
                                --indentlevel4;
                            }

                            // There is a "break" or "return" inside the loop.
                            // TODO: there can be false negatives. There could still be
                            //       execution paths that are not properly terminated
                            else if (tok4->str() == "break" || tok4->str() == "return")
                                break;
                        }
                    }
                }
            }
        }
    }
}

void CheckNullPointer::nullPointerStructByDeRefAndChec()
{
    // Dereferencing a struct pointer and then checking if it's NULL..

    // skipvar: don't check vars that has been tested against null already
    std::set<unsigned int> skipvar;
    skipvar.insert(0);

    // Scan through all tokens
    for (const Token *tok1 = _tokenizer->tokens(); tok1; tok1 = tok1->next()) {
        // Checking if some pointer is null.
        // then add the pointer to skipvar => is it known that it isn't NULL
        if (Token::Match(tok1, "if|while ( !| %var% )")) {
            tok1 = tok1->tokAt(2);
            if (tok1->str() == "!")
                tok1 = tok1->next();
            skipvar.insert(tok1->varId());
            continue;
        } else if (Token::Match(tok1, "( ! %var% %oror%") ||
                   Token::Match(tok1, "( %var% &&")) {
            // TODO: there are false negatives caused by this. The
            // variable should be removed from skipvar after the
            // condition
            tok1 = tok1->next();
            if (tok1->str() == "!")
                tok1 = tok1->next();
            skipvar.insert(tok1->varId());
            continue;
        }

        bool inconclusive = false;

        /**
         * @todo There are lots of false negatives here. A dereference
         *  is only investigated if a few specific conditions are met.
         */

        // dereference in assignment
        if (Token::Match(tok1, "[;{}] %var% . %var%")) {
            tok1 = tok1->next();
            if (tok1->strAt(3) == "(") {
                if (!_settings->inconclusive)
                    continue;
                inconclusive = true;
            }
        }

        // dereference in assignment
        else if (Token::Match(tok1, "[{};] %var% = %var% . %var%")) {
            if (tok1->strAt(1) == tok1->strAt(3))
                continue;
            tok1 = tok1->tokAt(3);
        }

        // dereference in condition
        else if (Token::Match(tok1, "if ( !| %var% .")) {
            tok1 = tok1->tokAt(2);
            if (tok1->str() == "!")
                tok1 = tok1->next();
        }

        // dereference in function call (but not sizeof|decltype)
        else if ((Token::Match(tok1->tokAt(-2), "%var% ( %var% . %var%") && !Token::Match(tok1->tokAt(-2), "sizeof|decltype ( %var% . %var%")) ||
                 Token::Match(tok1->previous(), ", %var% . %var%")) {
            // Is the function return value taken by the pointer?
            bool assignment = false;
            const unsigned int varid1(tok1->varId());
            if (varid1 == 0)
                continue;
            const Token *tok2 = tok1->previous();
            while (tok2 && !Token::Match(tok2, "[;{}]")) {
                if (Token::Match(tok2, "%varid% =", varid1)) {
                    assignment = true;
                    break;
                }
                tok2 = tok2->previous();
            }
            if (assignment)
                continue;

            // Is the dereference checked with a previous &&
            bool checked = false;
            for (tok2 = tok1->tokAt(-2); tok2; tok2 = tok2->previous()) {
                if (Token::Match(tok2, "[,(;{}]"))
                    break;
                else if (tok2->str() == ")")
                    tok2 = tok2->link();
                else if (Token::Match(tok2, "%varid% &&", varid1)) {
                    checked = true;
                    break;
                }
            }
            if (checked)
                continue;
        }

        // Goto next token
        else {
            continue;
        }

        // struct dereference was found - investigate if it is later
        // checked that it is not NULL
        const unsigned int varid1(tok1->varId());
        if (skipvar.find(varid1) != skipvar.end())
            continue;

        // name of struct pointer
        const std::string& varname(tok1->str());

        // is pointer local?
        bool isLocal = false;
        const Variable * var = _tokenizer->getSymbolDatabase()->getVariableFromVarId(tok1->varId());
        if (!var)
            continue;
        if (var->isLocal() || var->isArgument())
            isLocal = true;

        // member function may or may not nullify the pointer if it's global (#2647)
        if (!isLocal) {
            const Token *tok2 = tok1;
            while (Token::Match(tok2, "%var% ."))
                tok2 = tok2->tokAt(2);
            if (Token::Match(tok2,"%var% ("))
                continue;
        }

        // count { and } using tok2
        unsigned int indentlevel2 = 0;
        for (const Token *tok2 = tok1->tokAt(3); tok2; tok2 = tok2->next()) {
            if (tok2->str() == "{")
                ++indentlevel2;

            else if (tok2->str() == "}") {
                if (indentlevel2 == 0)
                    break;
                --indentlevel2;
            }

            // label / ?:
            else if (tok2->str() == ":")
                break;

            // function call..
            else if (Token::Match(tok2, "[;{}] %var% (") && CanFunctionAssignPointer(tok2->next(), varid1)) {
                if (!_settings->inconclusive)
                    break;
                inconclusive = true;
            }

            // Reassignment of the struct
            else if (tok2->varId() == varid1) {
                if (tok2->next()->str() == "=") {
                    // Avoid false positives when there is 'else if'
                    // TODO: can this be handled better?
                    if (tok1->strAt(-2) == "if")
                        skipvar.insert(varid1);
                    break;
                }
                if (Token::Match(tok2->tokAt(-2), "[,(] &"))
                    break;
            }

            // Loop..
            /** @todo don't bail out if the variable is not used in the loop */
            else if (tok2->str() == "do")
                break;

            // return/break at base level => stop checking
            else if (indentlevel2 == 0 && (tok2->str() == "return" || tok2->str() == "break"))
                break;

            // Function call: If the pointer is not a local variable it
            // might be changed by the call.
            else if (Token::Match(tok2, "[;{}] %var% (") &&
                     Token::simpleMatch(tok2->linkAt(2), ") ;") && !isLocal) {
                break;
            }

            // Check if pointer is null.
            // TODO: false negatives for "if (!p || .."
            else if (!tok2->isExpandedMacro() && Token::Match(tok2, "if ( !| %varid% )|&&", varid1)) {
                // Is this variable a pointer?
                if (var->isPointer())
                    nullPointerError(tok1, varname, tok2->linenr(), inconclusive);
                break;
            }
        }
    }
}

void CheckNullPointer::nullPointerByDeRefAndChec()
{
    const SymbolDatabase *symbolDatabase = _tokenizer->getSymbolDatabase();

    // Dereferencing a pointer and then checking if it's NULL..
    // This check will first scan for the check. And then scan backwards
    // from the check, searching for dereferencing.
    for (std::list<Scope>::const_iterator i = symbolDatabase->scopeList.begin(); i != symbolDatabase->scopeList.end(); ++i) {
        // TODO: false negatives.
        // - logical operators
        // - while
        const Token* const tok = i->classDef;
        if (i->type == Scope::eIf && tok && !tok->isExpandedMacro() && Token::Match(tok, "if ( !| %var% )|%oror%|&&")) {
            const Token * vartok = tok->tokAt(2);
            if (vartok->str() == "!")
                vartok = vartok->next();

            // Variable id for pointer
            const unsigned int varid(vartok->varId());
            if (varid == 0)
                continue;

            // Name of pointer
            const std::string varname(vartok->str());

            const Variable* var = symbolDatabase->getVariableFromVarId(varid);
            // Check that variable is a pointer..
            if (!var || !var->isPointer())
                continue;

            const Token * const decltok = var->nameToken();
            bool inconclusive = false;

            for (const Token *tok1 = tok->previous(); tok1 && tok1 != decltok; tok1 = tok1->previous()) {
                if (tok1->str() == ")" && Token::Match(tok1->link()->previous(), "%var% (")) {
                    const Token *tok2 = tok1->link();
                    while (tok2 && !Token::Match(tok2, "[;{}?:]"))
                        tok2 = tok2->previous();
                    if (Token::Match(tok2, "[?:]"))
                        break;
                    if (Token::Match(tok2, "[;{}] %varid% = %var%", varid))
                        break;

                    if (Token::Match(tok1->link()->previous(), "while ( %varid%", varid))
                        break;

                    if (Token::Match(tok1->link(), "( ! %varid% %oror%", varid) ||
                        Token::Match(tok1->link(), "( %varid% &&", varid)) {
                        tok1 = tok1->link();
                        continue;
                    }

                    if (Token::simpleMatch(tok1->link()->previous(), "sizeof (")) {
                        tok1 = tok1->link()->previous();
                        continue;
                    }

                    if (Token::Match(tok2, "[;{}] %var% ( %varid% ,", varid)) {
                        std::list<const Token *> varlist;
                        parseFunctionCall(*(tok2->next()), varlist, 0);
                        if (!varlist.empty() && varlist.front() == tok2->tokAt(3)) {
                            nullPointerError(tok2->tokAt(3), varname, tok->linenr(), inconclusive);
                            break;
                        }
                    }

                    // Passing pointer as parameter..
                    if (Token::Match(tok2, "[;{}] %type% (")) {
                        if (CanFunctionAssignPointer(tok2->next(), varid)) {
                            if (!_settings->inconclusive)
                                break;
                            inconclusive = true;
                        }
                    }

                    // calling unknown function => it might initialize the pointer
                    if (!(var->isLocal() || var->isArgument()))
                        break;
                }

                if (tok1->str() == "break")
                    break;

                if (tok1->varId() == varid) {
                    // Don't write warning if the dereferencing is
                    // guarded by ?: or &&
                    const Token *tok2 = tok1->previous();
                    if (tok2 && (tok2->isArithmeticalOp() || tok2->str() == "(")) {
                        while (tok2 && !Token::Match(tok2, "[;{}?:]")) {
                            if (tok2->str() == ")")
                                tok2 = tok2->link();
                            // guarded by &&
                            if (tok2->varId() == varid && tok2->next()->str() == "&&")
                                break;
                            tok2 = tok2->previous();
                        }
                    }
                    if (Token::Match(tok2, "[?:]") || tok2->varId() == varid)
                        continue;

                    // unknown : this is set by isPointerDeRef if it is
                    //           uncertain
                    bool unknown = _settings->inconclusive;

                    // reassign : is the pointer reassigned like this:
                    //            tok = tok->next();
                    bool reassign = false;
                    if (Token::Match(tok1->previous(), "= %varid% .", varid)) {
                        const Token *back = tok1->tokAt(-2);
                        while (back) {
                            if (back->varId() == varid) {
                                reassign = true;
                                break;
                            }
                            if (Token::Match(back, "[{};,(]")) {
                                break;
                            }
                            back = back->previous();
                        }
                    }

                    if (reassign) {
                        break;
                    } else if (Token::simpleMatch(tok1->tokAt(-2), "* )") &&
                               Token::Match(tok1->linkAt(-1)->tokAt(-2), "%varid% = (", tok1->varId())) {
                        break;
                    } else if (Token::simpleMatch(tok1->tokAt(-3), "* ) (") &&
                               Token::Match(tok1->linkAt(-2)->tokAt(-2), "%varid% = (", tok1->varId())) {
                        break;
                    } else if (Token::Match(tok1->previous(), "&&|%oror%")) {
                        break;
                    } else if (Token::Match(tok1->tokAt(-2), "&&|%oror% !")) {
                        break;
                    } else if (CheckNullPointer::isPointerDeRef(tok1, unknown, symbolDatabase)) {
                        nullPointerError(tok1, varname, tok->linenr(), inconclusive);
                        break;
                    } else if (tok1->strAt(-1) == "&") {
                        break;
                    } else if (tok1->strAt(1) == "=") {
                        break;
                    }
                }

                else if (tok1->str() == "{" ||
                         tok1->str() == "}")
                    break;

                // label..
                else if (Token::Match(tok1, "%type% :"))
                    break;
            }
        }
    }
}

void CheckNullPointer::nullPointerByCheckAndDeRef()
{
    const SymbolDatabase *symbolDatabase = _tokenizer->getSymbolDatabase();

    // Check if pointer is NULL and then dereference it..
    for (std::list<Scope>::const_iterator i = symbolDatabase->scopeList.begin(); i != symbolDatabase->scopeList.end(); ++i) {
        if (i->type != Scope::eIf && i->type != Scope::eElseIf && i->type != Scope::eWhile)
            continue;
        if (!i->classDef || i->classDef->isExpandedMacro())
            continue;

        const Token* const tok = i->type != Scope::eElseIf ? i->classDef->next() : i->classDef->tokAt(2);
        // TODO: investigate false negatives:
        // - handle "while"?
        // - if there are logical operators
        // - if (x) { } else { ... }

        // If the if-body ends with a unknown macro then bailout
        if (Token::Match(i->classEnd->tokAt(-3), "[;{}] %var% ;") && isUpper(i->classEnd->strAt(-2)))
            continue;

        // vartok : token for the variable
        const Token *vartok = 0;
        const Token *checkConditionStart = 0;
        if (Token::Match(tok, "( ! %var% )|&&")) {
            vartok = tok->tokAt(2);
            checkConditionStart = vartok->next();
        } else if (Token::Match(tok, "( %var% )|&&")) {
            vartok = tok->next();
        } else if (Token::Match(tok, "( ! ( %var% =")) {
            vartok = tok->tokAt(3);
            if (Token::simpleMatch(tok->tokAt(2)->link(), ") &&"))
                checkConditionStart = tok->tokAt(2)->link();
        } else
            continue;

        // variable id for pointer
        const unsigned int varid(vartok->varId());
        if (varid == 0)
            continue;

        // Check if variable is a pointer
        const Variable* var = _tokenizer->getSymbolDatabase()->getVariableFromVarId(varid);
        if (!var || !var->isPointer())
            continue;

        if (Token::Match(vartok->next(), "&& ( %varid% =", varid))
            continue;

        // Name and line of the pointer
        const std::string &pointerName = vartok->str();
        const unsigned int linenr = vartok->linenr();

        // if this is true then it is known that the pointer is null
        bool null = true;

        // Check the condition (eg. ( !x && x->i )
        if (checkConditionStart) {
            const Token * const conditionEnd = tok->link();
            for (const Token *tok2 = checkConditionStart; tok2 != conditionEnd; tok2 = tok2->next()) {
                // If we hit a || operator, abort
                if (tok2->str() == "||") {
                    break;
                }
                // Pointer is used
                else if (Token::Match(tok2, "* %varid%", varid)) {
                    nullPointerError(tok2->tokAt(2), pointerName, linenr, false);
                    break;
                }
                // Pointer is used
                else if (Token::Match(tok2, "%varid% .", varid)) {
                    nullPointerError(tok2, pointerName, linenr, false);
                    break;
                }
            }
        }

        // start token = inside the if-body
        const Token *tok1 = i->classStart;

        if (Token::Match(tok, "( %var% )|&&")) {
            // pointer might be null
            null = false;

            // start token = first token after the if/while body
            tok1 = i->classEnd->next();
            if (!tok1)
                continue;
        }

        unsigned int indentlevel = 0;

        // Set to true if we would normally bail out the check.
        bool inconclusive = false;

        // Count { and } for tok2
        for (const Token *tok2 = tok1; tok2; tok2 = tok2->next()) {
            if (tok2->str() == "{")
                ++indentlevel;
            else if (tok2->str() == "}") {
                if (indentlevel == 0)
                    break;
                --indentlevel;

                // calling exit function?
                bool unknown = false;
                if (_tokenizer->IsScopeNoReturn(tok2, &unknown)) {
                    if (_settings->inconclusive && unknown)
                        inconclusive = true;
                    else
                        break;
                }

                if (null && indentlevel == 0) {
                    // skip all "else" blocks because they are not executed in this execution path
                    while (Token::simpleMatch(tok2, "} else {"))
                        tok2 = tok2->linkAt(2);
                    null = false;
                }
            }

            if (tok2->str() == "return" || tok2->str() == "throw") {
                bool unknown = _settings->inconclusive;
                for (; tok2 && tok2->str() != ";"; tok2 = tok2->next()) {
                    if (tok2->varId() == varid) {
                        if (CheckNullPointer::isPointerDeRef(tok2, unknown, symbolDatabase))
                            nullPointerError(tok2, pointerName, linenr, inconclusive);
                        else if (unknown)
                            nullPointerError(tok2, pointerName, linenr, true);
                    }
                }
                break;
            }

            if (Token::Match(tok2, "goto|continue|break|if|switch|for"))
                break;

            // parameters to sizeof are not dereferenced
            if (Token::Match(tok2, "decltype|sizeof")) {
                if (tok2->strAt(1) != "(")
                    tok2 = tok2->next();
                else
                    tok2 = tok2->next()->link();
                continue;
            }

            // function call, check if pointer is dereferenced
            if (Token::Match(tok2, "%var% (")) {
                std::list<const Token *> vars;
                parseFunctionCall(*tok2, vars, 0);
                for (std::list<const Token *>::const_iterator it = vars.begin(); it != vars.end(); ++it) {
                    if (Token::Match(*it, "%varid% [,)]", varid)) {
                        nullPointerError(*it, pointerName, linenr, inconclusive);
                        break;
                    }
                }
            }

            // calling unknown function (abort/init)..
            else if (Token::simpleMatch(tok2, ") ;") &&
                     (Token::Match(tok2->link()->tokAt(-2), "[;{}.] %var% (") ||
                      Token::Match(tok2->link()->tokAt(-5), "[;{}] ( * %var% ) ("))) {
                // noreturn function?
                bool unknown = false;
                if (_tokenizer->IsScopeNoReturn(tok2->tokAt(2), &unknown)) {
                    if (!unknown || !_settings->inconclusive) {
                        break;
                    }
                    inconclusive = _settings->inconclusive;
                }

                // init function (global variables)
                if (!var || !(var->isLocal() || var->isArgument()))
                    break;
            }

            if (tok2->varId() == varid) {
                // unknown: this is set to true by isPointerDeRef if
                //          the function fails to determine if there
                //          is a dereference or not
                bool unknown = _settings->inconclusive;

                if (Token::Match(tok2->previous(), "[;{}=] %var% = 0 ;"))
                    ;

                else if (CheckNullPointer::isPointerDeRef(tok2, unknown, symbolDatabase))
                    nullPointerError(tok2, pointerName, linenr, inconclusive);

                else if (unknown && _settings->inconclusive)
                    nullPointerError(tok2, pointerName, linenr, true);

                else
                    break;
            }
        }
    }
}


void CheckNullPointer::nullPointer()
{
    nullPointerAfterLoop();
    nullPointerLinkedList();
    nullPointerStructByDeRefAndChec();
    nullPointerByDeRefAndChec();
    nullPointerByCheckAndDeRef();
}

/** Dereferencing null constant (simplified token list) */
void CheckNullPointer::nullConstantDereference()
{
    const SymbolDatabase *symbolDatabase = _tokenizer->getSymbolDatabase();

    for (std::list<Scope>::const_iterator i = symbolDatabase->scopeList.begin(); i != symbolDatabase->scopeList.end(); ++i) {
        if (i->type != Scope::eFunction || !i->classStart)
            continue;

        const Token *tok = i->classStart;

        if (i->function && (i->function->type == Function::eConstructor || i->function->type == Function::eCopyConstructor))
            tok = i->function->token; // Check initialization list

        for (; tok != i->classEnd; tok = tok->next()) {
            if (Token::Match(tok, "sizeof|decltype|typeid ("))
                tok = tok->next()->link();

            else if (Token::simpleMatch(tok, "* 0")) {
                if (Token::Match(tok->previous(), "return|throw|;|{|}|:|[|(|,") || tok->previous()->isOp() || tok->previous()->isAssignmentOp()) {
                    nullPointerError(tok);
                }
            }

            else if (Token::Match(tok, "0 [") && (tok->previous()->str() != "&" || !Token::Match(tok->next()->link()->next(), "[.(]")))
                nullPointerError(tok);

            else if (Token::Match(tok->previous(), "!!. %var% (") && (tok->previous()->str() != "::" || tok->strAt(-2) == "std")) {
                if (Token::simpleMatch(tok->tokAt(2), "0 )") && tok->varId()) { // constructor call
                    const Variable* var = symbolDatabase->getVariableFromVarId(tok->varId());
                    if (var && !var->isPointer() && !var->isArray() && Token::Match(var->typeStartToken(), "const| std :: string !!::"))
                        nullPointerError(tok);
                } else { // function call
                    std::list<const Token *> var;
                    parseFunctionCall(*tok, var, 0);

                    // is one of the var items a NULL pointer?
                    for (std::list<const Token *>::const_iterator it = var.begin(); it != var.end(); ++it) {
                        if (Token::Match(*it, "0 [,)]")) {
                            nullPointerError(*it);
                        }
                    }
                }
            } else if (Token::simpleMatch(tok, "std :: string ( 0 )"))
                nullPointerError(tok);

            else if (Token::simpleMatch(tok->previous(), ">> 0")) { // Only checking input stream operations is safe here, because otherwise 0 can be an integer as well
                const Token* tok2 = tok->previous(); // Find start of statement
                for (; tok2; tok2 = tok2->previous()) {
                    if (Token::Match(tok2->previous(), ";|{|}|:"))
                        break;
                }
                if (Token::simpleMatch(tok2, "std :: cin"))
                    nullPointerError(tok);
                if (tok2 && tok2->varId() != 0) {
                    const Variable* var = symbolDatabase->getVariableFromVarId(tok2->varId());
                    if (var && Token::Match(var->typeStartToken(), "const| std :: istream|ifstream|istringstream|stringstream|fstream|iostream"))
                        nullPointerError(tok);
                }
            }

            unsigned int ovarid = 0;
            if (Token::Match(tok, "0 ==|!= %var%"))
                ovarid = tok->tokAt(2)->varId();
            else if (Token::Match(tok, "%var% ==|!= 0"))
                ovarid = tok->varId();
            else if (Token::Match(tok, "%var% =|+=|+ 0 )|]|,|;|+"))
                ovarid = tok->varId();
            if (ovarid) {
                const Variable* var = symbolDatabase->getVariableFromVarId(ovarid);
                if (var && !var->isPointer() && !var->isArray() && Token::Match(var->typeStartToken(), "const| std :: string !!::"))
                    nullPointerError(tok);
            }
        }
    }
}


/// @addtogroup Checks
/// @{


/**
 * @brief %Check for null pointer usage (using ExecutionPath)
 */

class Nullpointer : public ExecutionPath {
public:
    /** Startup constructor */
    Nullpointer(Check *c, const SymbolDatabase* symbolDatabase_) : ExecutionPath(c, 0), symbolDatabase(symbolDatabase_), null(false) {
    }

private:
    const SymbolDatabase* symbolDatabase;

    /** Create checking of specific variable: */
    Nullpointer(Check *c, const unsigned int id, const std::string &name, const SymbolDatabase* symbolDatabase_)
        : ExecutionPath(c, id),
          symbolDatabase(symbolDatabase_),
          varname(name),
          null(false) {
    }

    /** Copy this check */
    ExecutionPath *copy() {
        return new Nullpointer(*this);
    }

    /** no implementation => compiler error if used by accident */
    void operator=(const Nullpointer &);

    /** is other execution path equal? */
    bool is_equal(const ExecutionPath *e) const {
        const Nullpointer *c = static_cast<const Nullpointer *>(e);
        return (varname == c->varname && null == c->null);
    }

    /** variable name for this check (empty => dummy check) */
    const std::string varname;

    /** is this variable null? */
    bool null;

    /** variable is set to null */
    static void setnull(std::list<ExecutionPath *> &checks, const unsigned int varid) {
        std::list<ExecutionPath *>::iterator it;
        for (it = checks.begin(); it != checks.end(); ++it) {
            Nullpointer *c = dynamic_cast<Nullpointer *>(*it);
            if (c && c->varId == varid)
                c->null = true;
        }
    }

    /**
     * Dereferencing variable. Check if it is safe (if the variable is null there's an error)
     * @param checks Checks
     * @param tok token where dereferencing happens
     */
    static void dereference(std::list<ExecutionPath *> &checks, const Token *tok) {
        const unsigned int varid(tok->varId());

        std::list<ExecutionPath *>::iterator it;
        for (it = checks.begin(); it != checks.end(); ++it) {
            Nullpointer *c = dynamic_cast<Nullpointer *>(*it);
            if (c && c->varId == varid && c->null) {
                if ((Token::Match(tok->tokAt(-4), "[=[(,] %var% && * %var% ,|]|)|;|=|%op%") ||
                     Token::Match(tok->tokAt(-5), "[=[(,] ! %var% %oror% * %var% ,|]|)|;|=|%op%") ||
                     Token::Match(tok->tokAt(-4), "return|case %var% && * %var% ,|:|;|=|%op%") ||
                     Token::Match(tok->tokAt(-5), "return|case ! %var% %oror% * %var% ,|:|;|=|%op%")) &&
                    tok->tokAt(-3) && (tok->tokAt(-3)->varId() == tok->varId()))
                    return;

                CheckNullPointer *checkNullPointer = dynamic_cast<CheckNullPointer *>(c->owner);
                if (checkNullPointer) {
                    checkNullPointer->nullPointerError(tok, c->varname);
                    return;
                }
            }
        }
    }

    /** parse tokens */
    const Token *parse(const Token &tok, std::list<ExecutionPath *> &checks) const {
        if (tok.varId() != 0) {
            // Pointer declaration declaration?
            const Variable* var = symbolDatabase->getVariableFromVarId(tok.varId());
            if (var && var->isPointer() && var->nameToken() == &tok)
                checks.push_back(new Nullpointer(owner, var->varId(), var->name(), symbolDatabase));
        }

        if (Token::simpleMatch(&tok, "try {")) {
            // Bail out all used variables
            const Token* tok2 = &tok;
            const Token* endtok = tok.linkAt(1);
            for (; tok2 && tok2 != endtok; tok2 = tok2->next()) {
                if (tok2->varId())
                    bailOutVar(checks,tok2->varId());
            }
            return tok2;
        }

        if (Token::Match(&tok, "%var% (")) {
            if (tok.str() == "sizeof" || tok.str() == "typeid")
                return tok.next()->link();

            // parse usage..
            std::list<const Token *> var;
            CheckNullPointer::parseFunctionCall(tok, var, 0);
            for (std::list<const Token *>::const_iterator it = var.begin(); it != var.end(); ++it)
                dereference(checks, *it);
        }

        else if (Token::simpleMatch(&tok, "( 0 &&"))
            return tok.link();

        if (tok.varId() != 0) {
            // unknown: if isPointerDeRef fails to determine if there
            //          is a dereference this will be set to true.
            bool unknown = owner->inconclusiveFlag();
            bool deref = CheckNullPointer::isPointerDeRef(&tok, unknown, symbolDatabase);

            if (deref)
                dereference(checks, &tok);
            else if (unknown && owner->inconclusiveFlag())
                dereference(checks, &tok);
            if (Token::Match(tok.previous(), "[;{}=] %var% = 0 ;"))
                setnull(checks, tok.varId());
            else if (!deref &&
                     (!tok.previous()->isOp() || tok.previous()->str() == "&") && !tok.previous()->isAssignmentOp() &&
                     (!tok.next()->isOp() || tok.next()->str() == ">>"))
                bailOutVar(checks, tok.varId()); // If its possible that the pointers value changes, bail out.
        }

        else if (tok.str() == "delete") {
            const Token *ret = tok.next();
            if (Token::simpleMatch(ret, "[ ]"))
                ret = ret->tokAt(2);
            if (Token::Match(ret, "%var% ;"))
                return ret->next();
        }

        else if (tok.str() == "return") {
            bool unknown = owner->inconclusiveFlag();
            const Token* tok2 = &tok;
            for (; tok2 && tok2->str() != ";"; tok2 = tok2->next()) {
                if (tok2->varId()) {
                    if (CheckNullPointer::isPointerDeRef(tok2, unknown, symbolDatabase) || unknown)
                        dereference(checks, tok2);
                }

                // If return statement contains "?" then assume there
                // is no dangours dereferencing later
                if (tok2->str() == "?") {
                    while (tok2 && tok2->str() != ";")
                        tok2 = tok2->next();
                    return tok2;
                }
            }
            return tok2;
        }

        return &tok;
    }

    /** parse condition. @sa ExecutionPath::parseCondition */
    bool parseCondition(const Token &tok, std::list<ExecutionPath *> &checks) {
        for (const Token *tok2 = &tok; tok2; tok2 = tok2->next()) {
            if (tok2->str() == "(" || tok2->str() == ")" || tok2->str() == "&&" || tok2->str() == "||")
                break;
            bool unknown = owner->inconclusiveFlag();
            if (tok2->varId() && (CheckNullPointer::isPointerDeRef(tok2, unknown, symbolDatabase) || unknown))
                dereference(checks, tok2);
        }

        if (Token::Match(&tok, "!| %var% (")) {
            std::list<const Token *> var;
            CheckNullPointer::parseFunctionCall(tok.str() == "!" ? *tok.next() : tok, var, 0);
            for (std::list<const Token *>::const_iterator it = var.begin(); it != var.end(); ++it)
                dereference(checks, *it);
        }

        return ExecutionPath::parseCondition(tok, checks);
    }


    void parseLoopBody(const Token *tok, std::list<ExecutionPath *> &checks) const {
        while (tok) {
            if (Token::Match(tok, "{|}|return|goto|break|if"))
                return;
            const Token *next = parse(*tok, checks);
            if (next)
                tok = tok->next();
        }
    }

};
/// @}


void CheckNullPointer::executionPaths()
{
    // Check for null pointer errors..
    Nullpointer c(this, _tokenizer->getSymbolDatabase());
    checkExecutionPaths(_tokenizer->getSymbolDatabase(), &c);
}

void CheckNullPointer::nullPointerError(const Token *tok)
{
    reportError(tok, Severity::error, "nullPointer", "Null pointer dereference");
}

void CheckNullPointer::nullPointerError(const Token *tok, const std::string &varname)
{
    reportError(tok, Severity::error, "nullPointer", "Possible null pointer dereference: " + varname);
}

void CheckNullPointer::nullPointerError(const Token *tok, const std::string &varname, const unsigned int line, bool inconclusive)
{
    const std::string errmsg("Possible null pointer dereference: " + varname + " - otherwise it is redundant to check if " + varname + " is null at line " + MathLib::toString<unsigned int>(line));
    reportError(tok, Severity::error, "nullPointer", errmsg, inconclusive);
}
