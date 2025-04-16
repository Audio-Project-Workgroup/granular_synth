#include "context.h"
#include "types.h"
#include "utils.h"
#include "arena.h"
#include "strings.h"

#include <stdio.h>
#include <stdlib.h>

//
// NOTE: os file handling
//

#if OS_WINDOWS

#include <windows.h>

static String8
readFile(Arena *allocator, String8 filename)
{
  String8 result = {};
  
  HANDLE fileHandle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
  if(fileHandle != INVALID_HANDLE_VALUE)
    {
      LARGE_INTEGER fileSize;
      if(GetFileSizeEx(fileHandle, &fileSize))
	{
	  ASSERT(fileSize.QuadPart);
	  result.size = fileSize.QuadPart;
	  result.str = arenaPushSize(allocator, result.size + 1);

	  u8 *dest = result.str;
	  usz totalBytesToRead = result.size;	  
	  while(totalBytesToRead)
	    {
	      u32 bytesToRead = totalBytesToRead & U32_MAX;
	      DWORD bytesRead;
	      if(ReadFile(fileHandle, dest, bytesToRead, &bytesRead, 0))
		{
		  dest += bytesRead;
		  totalBytesToRead -= bytesRead;
		}
	      else
		{
		  ASSERT(!"ReadFile() failed");
		}
	    }

	  if(result.str) result.str[result.size] = 0;
	}
      else
	{
	  ASSERT(!"GetFileSizeEx() failed");
	}

      CloseHandle(fileHandle);
    }
  else
    {
      ASSERT(!"invalid file handle value");
    }

  return(result);
}

static void
writeFile(String8 filename, String8 srcData)
{
  HANDLE fileHandle = CreateFileA(filename, GENERIC_WRITE, FILE_SHARE_WRITE, 0, CREATE_ALWAYS, 0, 0);

  if(fileHandle != INVALID_HANDLE_VALUE)
    {
      u8 *src = srcData.str;
      usz totalBytesToWrite = srcData.size;
      while(totalBytesToWrite)
	{
	  u32 bytesToWrite = totalBytesToWrite & U32_MAX;
	  if(WriteFile(fileHandle, src, bytesToWrite, 0, 0))
	    {
	      totalBytesToWrite -= bytesToWrite;
	      src += bytesToWrite;
	    }
	  else
	    {
	      ASSERT(!"WriteFile() failed");
	    }
	}
    }
  else
    {
      ASSERT(!"invalid file handle value");
    }
}

#elif OS_LINUX || OS_MAC

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

static String8
readFile(Arena *allocator, String8 filename)
{
  String8 result = {};

  int fileHandle = open((const char *)filename.str, O_RDONLY);
  if(fileHandle != -1)
    {
      struct stat fileStatus;
      if(fstat(fileHandle, &fileStatus) != -1)
	{
	  result.size = fileStatus.st_size;
	  result.str = arenaPushSize(allocator, result.size + 1);

	  u8 *dest = result.str;
	  usz totalBytesToRead = result.size;
	  while(totalBytesToRead)
	    {
	      u32 bytesToRead = MIN(totalBytesToRead, 0x7FFFF000) & U32_MAX;
	      ssize_t bytesRead = read(fileHandle, dest, bytesToRead);
	      if(bytesRead == bytesToRead)
		{
		  dest += bytesRead;
		  totalBytesToRead -= bytesRead;
		}
	      else
		{
		  ASSERT(!"read() failed");
		}
	    }

	  if(result.str) result.str[result.size] = 0;
	}
      else
	{
	  ASSERT(!"fstat() failed");
	}

      close(fileHandle);
    }
  else
    {
      ASSERT(!"open failed");
    }

  return(result);
}

static void
writeFile(String8 filename, String8 srcData)
{
  int fileHandle = open((const char *)filename.str,
			O_CREAT | O_WRONLY | O_TRUNC,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if(fileHandle != -1)
    {
      u8 *src = srcData.str;
      usz totalBytesToWrite = srcData.size;
      while(totalBytesToWrite)
	{
	  u32 bytesToWrite = MIN(totalBytesToWrite, 0x7FFFF000) & U32_MAX;
	  ssize_t bytesWritten = write(fileHandle, src, bytesToWrite);
	  if(bytesWritten == bytesToWrite)
	    {
	      totalBytesToWrite -= bytesWritten;
	      src += bytesWritten;
	    }
	  else
	    {
	      ASSERT(!"write() failed");
	    }
	}

      close(fileHandle);
    }
  else
    {
      ASSERT(!"open() failed");
    }
}

#endif

//
// NOTE: stream advance/test functions
//

static bool
isValid(String8 input)
{
  bool result = input.size > 0;

  return(result);
}

static String8
advanceByte(String8 input)
{
  String8 result = {};
  if(input.size)
    {
      result.str = input.str + 1;
      result.size = input.size - 1;
    }

  return(result);
}

static bool
peekByte(String8 input, u8 c)
{
  bool result = false;
  if(input.size > 1)
    {
      result = (*(input.str + 1) == c);
    }

  return(result);
}

static String8
seekByte(String8 input, u8 c)
{
  String8 result = input;
  while(!peekByte(result, c) && isValid(result))
    {
      result = advanceByte(result);
    }
  result = advanceByte(result);
  ASSERT(*result.str == c);
  result = advanceByte(result);

  return(result);
}

static String8
seekString(String8 input, String8 str)
{
  String8 result = input;
  while(isValid(result))
    {
      if(stringsAreEqual({result.str, str.size}, str))
	{
	  break;
	  //result = advanceByte(result);
	}
      else
	{
	  result = advanceByte(result);
	}
    }
  
  ASSERT(stringsAreEqual({result.str, str.size}, str));
  for(u64 count = 0; count < str.size; ++count)
    {
      result = advanceByte(result);
    }

  return(result);
}

static bool
isWhitespace(u8 c)
{
  bool result = ((c == ' ') ||
		 (c == '\n') ||
		 (c == '\t') ||
		 (c == '\r') ||
		 (c == '\v') ||
		 (c == '\f'));

  return(result);
}

static bool
isAlphabetic(u8 c)
{
  bool result = (((c >= 'a') && (c <= 'z')) ||
		 ((c >= 'A') && (c <= 'Z')));

  return(result);
}

static bool
isNumeric(u8 c)
{
  bool result = ((c >= '0') && (c <= '9'));

  return(result);
}

static String8
eatWhitespace(String8 input)
{
  String8 result = input;
  while(isWhitespace(*result.str)) result = advanceByte(result);

  return(result);
}

//
// NOTE: tokens
//

#define TOKEN_XLIST \
  X(unknown) \
  X(identifier) \
  X(keyword)	  \
  X(floatLiteral) \
  X(integerLiteral) \
  X(stringLiteral) \
  X(lParen) \
  X(rParen) \
  X(lBracket) \
  X(rBracket) \
  X(lBrace) \
  X(rBrace) \
  X(lt) \
  X(gt) \
  X(semicolon) \
  X(asterisk) \
  X(slash) \
  X(backslash) \
  X(period)    \
  X(comma) \
  X(hyphen) \
  X(plus) \
  X(percent) \
  X(equals) \
  X(exclam) \
  X(ampersand) \
  X(pipe) \
  X(caret) \
  X(tilde) \
  X(hash) \

enum TokenKind
{
#define X(name) Token_##name,
  TOKEN_XLIST
#undef X
};

static String8 tokenNames[]
  {
#define X(name) STR8_LIT(#name),
    TOKEN_XLIST
#undef X
  };

struct Token
{
  TokenKind kind;
  
  union
  {
    String8 name;
    u64 value_int;
    r64 value_float;
  };
};

struct TokenNode
{
  TokenNode *next;
  Token token;
};

struct TokenList
{
  TokenNode *first;
  TokenNode *last;
  u64 count;
};

static void
appendTokenToList(Arena *allocator, TokenList *list, Token token)
{
  TokenNode *node = arenaPushStruct(allocator, TokenNode);
  node->token = token;
  
  QUEUE_PUSH(list->first, list->last, node);
  ++list->count;
  //printf("DEBUG: list->count=%zu\n", list->count);
}

static Token
getTokenFromList(TokenList *list)
{
  TokenNode *node = list->first;
  Token result = node->token;
  QUEUE_POP(list->first, list->last);

  return(result);
}

static Token
peekToken(TokenList *list, TokenKind tokenKind)
{
  Token result = {};
  TokenNode *node = list->first;
  if(node)
    {
      Token token = node->token;
      if(token.kind == tokenKind)
	{
	  result = token;
	  QUEUE_POP(list->first, list->last);
	}
    }
  
  return(result);
}

static String8 keywords[]
  {
    STR8_LIT("struct"),
    STR8_LIT("enum"),
    STR8_LIT("union"),
    STR8_LIT("typedef"),
    STR8_LIT("inline"),
    STR8_LIT("static"),
    STR8_LIT("for"),
    STR8_LIT("while"),
    STR8_LIT("do"),
    STR8_LIT("if"),
    STR8_LIT("else"),
    STR8_LIT("switch"),
    STR8_LIT("case"),
    STR8_LIT("default"),
    STR8_LIT("break"),
    STR8_LIT("continue"),
    STR8_LIT("return"),
    STR8_LIT("defer"),
    STR8_LIT("goto"),
    STR8_LIT("sizeof"),
    STR8_LIT("volatile"),
  };

static void
printString(String8 string)
{
  u8 *at = string.str;
  for(u64 i = 0; i < string.size; ++i)
    {
      printf("%c", (char)*at++);
    }
}

static void
printToken(Token token)
{
  printString(tokenNames[token.kind]);
  if((token.kind == Token_identifier) ||
     (token.kind == Token_keyword) ||
     (token.kind == Token_stringLiteral))
    {
      printf(": ");
      printString(token.name);
    }
  else if(token.kind == Token_floatLiteral)
    {
      printf(": %f", token.value_float);
    }
  else if(token.kind == Token_integerLiteral)
    {
      printf(": %zu", token.value_int);
    }  
  printf("\n");
}

static TokenList
tokenizeFile(Arena *allocator, String8 input)
{
  TokenList result = {};

  String8 at = input;
  while(isValid(at))
    {
      Token newToken = {};

      at = eatWhitespace(at);
      if(isValid(at))
	{
	  if(isAlphabetic(*at.str) || (*at.str == '_'))
	    {	  
	      String8 identifierOrKeyword = {at.str, 0};
	      while(isAlphabetic(*at.str) ||
		    isNumeric(*at.str) ||
		    (*at.str == '_'))
		{
		  at = advanceByte(at);
		  ++identifierOrKeyword.size;
		}

	      bool isKeyword = false;
	      for(u32 keywordIndex = 0; keywordIndex < ARRAY_COUNT(keywords); ++keywordIndex)
		{
		  if(stringsAreEqual(identifierOrKeyword, keywords[keywordIndex]))
		    {
		      isKeyword = true;
		      newToken.kind = Token_keyword;
		      break;
		    }
		}
	  
	      if(!isKeyword) newToken.kind = Token_identifier;
	  
	      newToken.name = identifierOrKeyword;
	    }
	  else if(isNumeric(*at.str))
	    {	 	  
	      u64 integer = 0;
	      while(isNumeric(*at.str))
		{
		  integer *= 10;
		  integer += *at.str - '0';

		  at = advanceByte(at);
		}

	      if(*at.str == '.')
		{
		  at = advanceByte(at);
	      
		  if(isNumeric(*at.str))
		    {
		      u64 decimalDigits = 0;
		      u64 decimalPlaceCount = 0;
		      while(isNumeric(*at.str))
			{
			  ++decimalPlaceCount;
			  decimalDigits *= 10;
			  decimalDigits += *at.str;

			  at = advanceByte(at);
			}

		      r64 decimalFloat = (r64)decimalDigits;
		      for(u64 decimal = 0; decimal < decimalPlaceCount; ++decimal)
			{
			  decimalFloat /= 10.0; // TODO: this is super inaccurate
			}

		      newToken.kind = Token_floatLiteral;
		      newToken.value_float = (r64)integer + decimalFloat;
		    }
		  else if(*at.str == 'f')
		    {
		      newToken.kind = Token_floatLiteral;
		      newToken.value_float = (r64)integer;
		    }
		  else
		    {
		      // TODO: error?
		    }
		}
	      else
		{
		  newToken.kind = Token_integerLiteral;
		  newToken.value_int = integer;
		}
	    }
	  else if(*at.str == '\"')
	    {
	      newToken.kind = Token_stringLiteral;

	      at = advanceByte(at);
	      String8 stringLiteral = {at.str, 0};
	      while(*at.str != '\"')
		{
		  ++stringLiteral.size;
		  at = advanceByte(at);
		}
	      ASSERT(*at.str == '\"');
	      at = advanceByte(at);

	      newToken.name = stringLiteral;
	    }
	  else if(*at.str == '(')
	    {
	      newToken.kind = Token_lParen;
	      at = advanceByte(at);
	    }
	  else if(*at.str == ')')
	    {
	      newToken.kind = Token_rParen;
	      at = advanceByte(at);
	    }
	  else if(*at.str == '[')
	    {
	      newToken.kind = Token_lBracket;
	      at = advanceByte(at);
	    }
	  else if(*at.str == ']')
	    {
	      newToken.kind = Token_rBracket;
	      at = advanceByte(at);
	    }
	  else if(*at.str == '{')
	    {
	      newToken.kind = Token_lBrace;
	      at = advanceByte(at);
	    }
	  else if(*at.str == '}')
	    {
	      newToken.kind = Token_rBrace;
	      at = advanceByte(at);
	    }
	  else if(*at.str == ';')
	    {
	      newToken.kind = Token_semicolon;
	      at = advanceByte(at);
	    }
	  else if(*at.str == '*')
	    {
	      newToken.kind = Token_asterisk;
	      at = advanceByte(at);
	    }
	  else if(*at.str == '/')
	    {
	      if(peekByte(at, '/'))
		{
		  // at = advanceByte(at);
		  // at = advanceByte(at);
		  at = seekByte(at, '\n');
		  continue;
		}
	      else if(peekByte(at, '*'))
		{
		  at = seekString(at, STR8_LIT("*/"));
		  continue;
		}
	      else
		{
		  newToken.kind = Token_slash;
		  at = advanceByte(at);
		}
	    }
	  else if(*at.str == '.')
	    {
	      newToken.kind = Token_period;
	      at = advanceByte(at);
	    }
	  else if(*at.str == ',')
	    {
	      newToken.kind = Token_comma;
	      at = advanceByte(at);
	    }
	  else if(*at.str == '<')
	    {
	      newToken.kind = Token_lt;
	      at = advanceByte(at);
	    }
	  else if(*at.str == '>')
	    {
	      newToken.kind = Token_gt;
	      at = advanceByte(at);
	    }
	  else if(*at.str == '\\')
	    {
	      newToken.kind = Token_backslash;
	      at = advanceByte(at);
	    }
	  else if(*at.str == '-')
	    {
	      newToken.kind = Token_hyphen;
	      at = advanceByte(at);
	    }
	  else if(*at.str == '+')
	    {
	      newToken.kind = Token_plus;
	      at = advanceByte(at);
	    }
	  else if(*at.str == '=')
	    {
	      newToken.kind = Token_equals;
	      at = advanceByte(at);
	    }
	  else if(*at.str == '%')
	    {
	      newToken.kind = Token_percent;
	      at = advanceByte(at);
	    }
	  else if(*at.str == '!')
	    {
	      newToken.kind = Token_exclam;
	      at = advanceByte(at);
	    }
	  else if(*at.str == '&')
	    {
	      newToken.kind = Token_ampersand;
	      at = advanceByte(at);
	    }
	  else if(*at.str == '|')
	    {
	      newToken.kind = Token_pipe;
	      at = advanceByte(at);
	    }
	  else if(*at.str == '^')
	    {
	      newToken.kind = Token_caret;
	      at = advanceByte(at);
	    }
	  else if(*at.str == '~')
	    {
	      newToken.kind = Token_tilde;
	      at = advanceByte(at);
	    }
	  else if(*at.str == '#')
	    {
	      newToken.kind = Token_hash;
	      at = advanceByte(at);
	    }
	  else
	    {
	      // TODO: unknown token
	      at = advanceByte(at);
	    }

	  // printf("DEBUG: newToken: ");
	  // printToken(newToken);
	  // printf("\n");
	  appendTokenToList(allocator, &result, newToken);
	}
    }

  return(result);
}

struct ParsedStruct
{
  String8 name;  
  ParsedStruct *next;
};

static ParsedStruct *firstParsedStruct = 0;

struct UnknownStruct
{
  UnknownStruct *next;
  UnknownStruct *prev;
  String8 name;
};
static UnknownStruct *firstUnknownStruct = 0;
static UnknownStruct *lastUnknownStruct = 0;

//
// NOTE: parsing
//

struct ParseContext
{
  Arena *allocator;
  String8List *output;
  TokenList *tokens;

  bool inUnion;
};

static void parseMember(ParseContext *parseContext);
static void parseStruct(ParseContext *parseContext);
static void parseUnion(ParseContext *parseContext);

static void
parseUnion(ParseContext *parseContext)
{
  TokenList *tokens = parseContext->tokens;
  String8List *output = parseContext->output;
  Arena *allocator = parseContext->allocator;

  parseContext->inUnion = true;

  Token optionalNameToken = peekToken(tokens, Token_identifier);

  if(peekToken(tokens, Token_lBrace).kind != Token_unknown)
    {
      while(peekToken(tokens, Token_rBrace).kind == Token_unknown)
	{
	  parseMember(parseContext);
	}
      ASSERT(peekToken(tokens, Token_semicolon).kind != Token_unknown);
    }
  parseContext->inUnion = false;
}

static void
parseMember(ParseContext *parseContext)
{
  TokenList *tokens = parseContext->tokens;
  String8List *output = parseContext->output;
  Arena *allocator = parseContext->allocator;

  bool isPointer = false;

  Token optionalKeywordToken = peekToken(tokens, Token_keyword);
  if(optionalKeywordToken.kind != Token_unknown)
    {
      if(stringsAreEqual(optionalKeywordToken.name, STR8_LIT("union")))
	{
	  parseUnion(parseContext);
	  return;
	}
      else if(stringsAreEqual(optionalKeywordToken.name, STR8_LIT("struct")))
	{
	  parseStruct(parseContext);
	  return;
	}
    }

  Token typeToken = getTokenFromList(tokens);

  bool typeIsKnown = false;
  for(ParsedStruct *parsedStruct = firstParsedStruct; parsedStruct; parsedStruct = parsedStruct->next)
    {
      if(stringsAreEqual(parsedStruct->name, typeToken.name))
	{
	  typeIsKnown = true;
	  break;
	}
    }

  if(!typeIsKnown)
    {
      UnknownStruct *unknownStruct = arenaPushStruct(allocator, UnknownStruct);
      unknownStruct->name = typeToken.name;
      QUEUE_PUSH(firstUnknownStruct, lastUnknownStruct, unknownStruct);
    }
    
  if(peekToken(tokens, Token_asterisk).kind != Token_unknown)
    {
      isPointer = true;
    }
  
  Token nameToken = getTokenFromList(tokens);

  u64 arrayCount = 0;
  bool arrayCountIsIdentifier = false;
  String8 arrayCountIdentifier = {};
  if(peekToken(tokens, Token_lBracket).kind != Token_unknown)
    {
      Token countToken_literal = peekToken(tokens, Token_integerLiteral);
      if(countToken_literal.kind != Token_unknown)
	{
	  arrayCount = countToken_literal.value_int;
	}
      else 
	{
	  Token countToken_identifier = peekToken(tokens, Token_identifier);
	  if(countToken_identifier.kind != Token_unknown)
	    {
	      // TODO: get count from identifier
	      arrayCountIsIdentifier = true;
	      arrayCountIdentifier = countToken_identifier.name;
	    }
	}

      ASSERT(peekToken(tokens, Token_rBracket).kind != Token_unknown);
    }
  ASSERT(peekToken(tokens, Token_semicolon).kind != Token_unknown);

  if(arrayCountIsIdentifier)
    {
      stringListPushFormat(allocator, output,
			   "  {STR8_LIT(\"%.*s\"), STR8_LIT(\"%.*s\"), %d, %d, %.*s},\n",
			   typeToken.name.size, typeToken.name.str,
			   nameToken.name.size, nameToken.name.str,
			   isPointer,
			   parseContext->inUnion,
			   arrayCountIdentifier.size, arrayCountIdentifier.str);
    }
  else
    {
      stringListPushFormat(allocator, output,
			   "  {STR8_LIT(\"%.*s\"), STR8_LIT(\"%.*s\"), %d, %d, %d},\n",
			   typeToken.name.size, typeToken.name.str,
			   nameToken.name.size, nameToken.name.str,
			   isPointer, parseContext->inUnion, arrayCount);
    }
}

static void
parseStruct(ParseContext *parseContext)
{
  TokenList *tokens = parseContext->tokens;
  String8List *output = parseContext->output;
  Arena *allocator = parseContext->allocator;

  Token optionalNameToken = peekToken(tokens, Token_identifier);

  bool shouldParse = true;
  if(optionalNameToken.kind != Token_unknown)
    {
      for(ParsedStruct *parsedStruct = firstParsedStruct; parsedStruct; parsedStruct = parsedStruct->next)
	{
	  if(stringsAreEqual(optionalNameToken.name, parsedStruct->name))
	    {
	      shouldParse = false;
	      break;
	    }
	}
    }

  if(shouldParse)
    {
      if(peekToken(tokens, Token_lBrace).kind != Token_unknown)
	{
	  if(optionalNameToken.kind != Token_unknown)
	    {
	      stringListPushFormat(allocator, output,
				   "static MemberDefinition membersOf_%.*s[] = \n{\n",
				   (int)optionalNameToken.name.size, optionalNameToken.name.str);
	    }
      
	  while(peekToken(tokens, Token_rBrace).kind == Token_unknown)
	    {
	      parseMember(parseContext);
	    }
	  ASSERT(peekToken(tokens, Token_semicolon).kind != Token_unknown);

	  if(optionalNameToken.kind != Token_unknown)
	    {
	      stringListPushFormat(allocator, output, "};\n\n");
	    }
	}

      if(optionalNameToken.kind != Token_unknown)
	{      
	  ParsedStruct *newParsedStruct = arenaPushStruct(allocator, ParsedStruct);
	  newParsedStruct->name = arenaPushString(allocator, optionalNameToken.name);
	  newParsedStruct->next = 0;
	  STACK_PUSH(firstParsedStruct, newParsedStruct);
	}
    }
}

static void
parseIntrospectable(ParseContext *parseContext)
{
  Token token = getTokenFromList(parseContext->tokens);
  switch(token.kind)
    {
    case Token_unknown: {} break;
    case Token_keyword:
      {
	if(stringsAreEqual(token.name, STR8_LIT("struct")))
	  {
	    parseStruct(parseContext);
	  }
      } break;
    default:
      {
	//printToken(token);
      } break;
    }
}

static String8List cliFilenameList = {};
static String8List includedFilenameList = {};
static String8List parsedFilenameList = {};
static String8List loadedFileContents = {};

static void
parseFile(Arena *allocator, String8 fileContents)
{  
  TokenList tokens = tokenizeFile(allocator, fileContents);
  String8List outputList = {};
  ParseContext parseContext = {allocator, &outputList, &tokens};

  //for(u64 tokenIndex = 0; tokenIndex < tokens.count; ++tokenIndex)
  while(tokens.first)
    {
      // TokenNode *node = tokens.first;
      // Token token = node->token;
      // QUEUE_POP(tokens.first, tokens.last);
      Token token = getTokenFromList(&tokens);
      //printToken(token);
      switch(token.kind)
	{
	case Token_unknown: {} break;
	case Token_hash:
	  {
	    Token directiveToken = getTokenFromList(&tokens);
	    if(directiveToken.kind == Token_identifier)
	      {
		if(stringsAreEqual(directiveToken.name, STR8_LIT("include")))
		  {
		    peekToken(&tokens, Token_lt);
		    
		    Token includeFilenameToken = getTokenFromList(&tokens);
		    if(includeFilenameToken.kind == Token_stringLiteral)
		      {	       
			 stringListPush(allocator, &includedFilenameList, includeFilenameToken.name);
		      }
		    else if(includeFilenameToken.kind == Token_identifier)
		      {
			//stringListPush(allocator, &includedFilenameList, includeFilenameToken.name);
		      }
		    
		    peekToken(&tokens, Token_gt);
		  }
	      }
	  }
	case Token_identifier:
	  {
	    if(stringsAreEqual(token.name, STR8_LIT("INTROSPECT")))
	      {
		parseIntrospectable(&parseContext);
	      }
	  } break;
	case Token_keyword:
	  {
	    if(stringsAreEqual(token.name, STR8_LIT("struct")))	      
	      {
		Token nameToken = getTokenFromList(&tokens);
		bool structIsUnknown = false;
		for(UnknownStruct *unknownStruct = firstUnknownStruct;
		    unknownStruct && unknownStruct != lastUnknownStruct;
		    unknownStruct = unknownStruct->next)
		  {
		    if(stringsAreEqual(nameToken.name, unknownStruct->name))
		      {
			structIsUnknown = true;
			break;
		      }
		  }

		if(structIsUnknown)
		  {
		    TokenNode *nameTokenNode = arenaPushStruct(allocator, TokenNode);
		    nameTokenNode->token = nameToken;
		    QUEUE_PUSH_FRONT(tokens.first, tokens.last, nameTokenNode);
		    
		    parseStruct(&parseContext);
		  }
	      }
	  }
	default:
	  {
	    //printToken(token);
	  } break;
	}      
    }

  for(u64 stringIndex = 0; stringIndex < outputList.nodeCount; ++stringIndex)
    {
      String8Node *node = outputList.first;
      String8 string = node->string;
      fprintf(stdout, "%.*s", (int)string.size, string.str);
      QUEUE_POP(outputList.first, outputList.last);
    }

  // for(u64 includeIndex = 0; includeIndex < includedFilenameList.nodeCount; ++includeIndex)
  //   {
  //     String8Node *includeNode = includedFilenameList.first;
  //     String8 includeFilename = includeNode->string;
  //     String8 includeContents = readFile(allocator, includeFilename);
  //     //stringListPush(allocator, &loadedFileContents, includeContents);
  //     String8Node *includeContentsNode = arenaPushStruct(allocator, String8Node);
  //     includeContentsNode->string = includeContents;
  //     stringListPushNode(&loadedFileContents, includeContentsNode);

  //     QUEUE_POP(includedFilenameList.first, includedFilenameList.last);
  //   }
}

int main(int argc, char **argv)
{
  // NOTE: get filenames to parse
  usz filenameBufferSize = KILOBYTES(1);
  void *filenameBuffer = calloc(filenameBufferSize, 1);
  Arena filenameArena = arenaBegin(filenameBuffer, filenameBufferSize);
  //String8List filenameList = {};
  for(int argi = 1; argi < argc; ++argi)
    {
      stringListPush(&filenameArena, &cliFilenameList, STR8_CSTR(argv[argi]));
    }

  usz parseBufferSize = MEGABYTES(4);
  void *parseBuffer = calloc(parseBufferSize, 1);
  Arena parseArena = arenaBegin(parseBuffer, parseBufferSize);
  //for(u32 fileIndex = 0; fileIndex < filenameList.nodeCount; ++fileIndex)
  while(cliFilenameList.first)
    {      
      String8Node *fileNode = cliFilenameList.first;
      String8 filename = fileNode->string;
      String8 fileContents = readFile(&parseArena, filename);
      fprintf(stderr, "DEBUG: %.*s size=%zu\n", (int)filename.size, filename.str, fileContents.size);
      parseFile(&parseArena, fileContents);
      stringListPush(&filenameArena, &parsedFilenameList, filename);
      
      QUEUE_POP(cliFilenameList.first, cliFilenameList.last);
      //arenaEnd(&parseArena);
    }

  while(includedFilenameList.first)
    {
      String8Node *fileNode = includedFilenameList.first;
      String8 filename = fileNode->string;
      String8 fileContents = readFile(&parseArena, filename);
      fprintf(stderr, "DEBUG: %.*s size=%zu\n", (int)filename.size, filename.str, fileContents.size);

      bool includeFileAlreadyParsed = false;
      for(String8Node *node = parsedFilenameList.first; node; node = node->next)
	{
	  String8 string = node->string;
	  if(stringsAreEqual(filename, string))
	    {
	      includeFileAlreadyParsed = true;
	      break;
	    }
	}
			
      if(!includeFileAlreadyParsed)
	{
	  parseFile(&parseArena, fileContents);
	  stringListPush(&filenameArena, &parsedFilenameList, filename);
	}           
      
      QUEUE_POP(includedFilenameList.first, includedFilenameList.last);
    }

  if(firstParsedStruct)
    {
      fprintf(stdout, "static MetaStruct metaStructs[] = \n{\n");
      for(ParsedStruct *parsedStruct = firstParsedStruct; parsedStruct; parsedStruct = parsedStruct->next)
	{
	  fprintf(stdout, "  {STR8_LIT(\"%.*s\"), membersOf_%.*s},\n",
		  (int)parsedStruct->name.size, parsedStruct->name.str,
		  (int)parsedStruct->name.size, parsedStruct->name.str);
	}
      fprintf(stdout, "};\n\n");
    }

  while(firstUnknownStruct)
    {
      UnknownStruct *unknownStruct = firstUnknownStruct;
      fprintf(stderr, "DEBUG: unknown struct: `%.*s`\n",
	      (int)unknownStruct->name.size, unknownStruct->name.str);

      QUEUE_POP(firstUnknownStruct, lastUnknownStruct);
    }
}
