#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <Carbon/Carbon.h>

#include "BBLMInterface.h"
#include "BBXTInterface.h"
#include "BBLMTextIterator.h"


// DEBUGing macros
#if DEBUG
void debugf_(const char* func,const char* fileName,long line, const char*fmt,...)
{
	va_list arg;
	char msg[256];
	va_start(arg, fmt);
	vsnprintf(msg,256 ,fmt, arg);
    DebugAssert(COMPONENT_SIGNATURE, DEBUG_NO_OPTIONS, "BBPy.LM" ": " , msg, nil, fileName, line, 0 );

}
#define debugf(FMT,...)  debugf_( __FUNCTION__,__FILE__, __LINE__,FMT,__VA_ARGS__);
#else
#define debugf(FMT,...) 
#endif


#define HIGHLIGHT_HTML_IN_STRINGS 1


// Things in strings
enum
{
	kPyBBLMStringSubst =  kBBLMFirstUserRunKind,
	kPyBBLMStringTag,
	kPyBBLMStringTagAttr,
	kPyBBLMStringTagVal,
	kPyBBLMStringProcDir,
	kPyBBLMStringComment
};

#define iswordchar(x) (isalnum(x)||x=='_')

struct runloc
{
	UInt32 	pos;
	UInt32	last_start;
};

enum
{
	kBufsize = 3
};

typedef UniChar charbuf[kBufsize]; 

static UniChar start(struct runloc& r, BBLMTextIterator &p, BBLMParamBlock &pb, charbuf chars)
{
	r.last_start = pb.fCalcRunParams.fStartOffset;
	r.pos = pb.fCalcRunParams.fStartOffset;
	p += r.pos;
	
	if (chars)
	{
		for (int i=1; i <kBufsize; i++)
			chars[i] = 0;
			
		chars[0] = *p;
	}
	
	return *p;
}

static UniChar nextchar(struct runloc& r, BBLMTextIterator &p, BBLMParamBlock &pb, charbuf chars)
{
	if (chars)
	{
		for (int i=kBufsize-1; i > 0; i--)
			chars[i] = chars[i-1];
	}
	
	if (r.pos < pb.fTextLength)
	{
		r.pos++;
		p++;

		if (chars)
			chars[0] = *p;
			
		return *p;
	}

	if (chars)
		chars[0] = 0;
		
	return 0;
}

static bool addRun(BBLMRunCode kind, int  start,int len , const BBLMCallbackBlock& bblm_callbacks)
{
	if (len > 0){ // Tie off the code run we were in, unless the length is zero.
		debugf("Run %d %d:%d", kind, start, start+len-1 );
		return bblmAddRun(	&bblm_callbacks, 'Pyth',
							kind, start, len, false);
							
	}
	else{
		return true;
	}
}					




static bool addRunAt (BBLMRunCode kind, struct runloc& r, const BBLMCallbackBlock& bblm_callbacks, int off=0)
{
	bool more_runs = addRun(kind, r.last_start, r.pos - r.last_start+1+off, bblm_callbacks);
	r.last_start =  r.pos+1+off;
	return more_runs;
}



static bool colorstr(	UniChar delim,
				BBLMParamBlock &pb,
				struct runloc &r,
				BBLMTextIterator &p,
				const BBLMCallbackBlock &bblm_callbacks)
{
	enum
	{
		kLocalPyBBLMStringTag = kPyBBLMStringTag,
		kLocalBBLMRunIsSingleString = kBBLMRunIsSingleString
	};
	
	bool lookup = false, more_runs = true, tripple = false;
	bool tag=false,qtagval=false,wtagval=false,xmlprocdir = false;
	int htmlcomment=0;
	int delims = 0;
	charbuf chars;
	UniChar c;
	
	c = nextchar(r,p,pb,chars);

	if (c == delim){
		c =  nextchar(r, p, pb, chars);
		if (c == delim){
			tripple = true;
			c = nextchar(r, p, pb, chars);
		}  
		else{
			//double
			return addRunAt(kBBLMRunIsSingleString,r,bblm_callbacks,-1);
		}	
	}
	while (c && more_runs){
		if (c == delim && (chars[1] != '\\' || chars[2] == '\\')){
			if (tripple){
				if (++delims==3){
					break; // end of tripple-quote.
				}  
			}
			else{
				break; // end of single-quote.
			}
		}
		else{
			delims = 0;
		}
		if ((c=='\r'||c=='\n') && !tripple){
				break;
		}
		// Color python string subst things		
		else if (chars[2] != '%' && chars[1] == '%'){
			if (isalpha(c)){
				more_runs = addRunAt(tag?kLocalPyBBLMStringTag:kLocalBBLMRunIsSingleString,r,bblm_callbacks,-2)
					&& addRunAt(kPyBBLMStringSubst,r,bblm_callbacks);
			}
			else if (c == '('){
				more_runs = addRunAt(tag?kLocalPyBBLMStringTag:kLocalBBLMRunIsSingleString,r,bblm_callbacks,-2);
				lookup = true;
			}
		}
		else if (lookup){
			if (chars[1] ==')' && isalpha(c)){
				more_runs = addRunAt(kPyBBLMStringSubst,r,bblm_callbacks);
				lookup = false;
			}
		}
		// html in strings
#if HIGHLIGHT_HTML_IN_STRINGS		
		else if (tripple && !htmlcomment && chars[1] == '<' ){
			if (isalpha(c) || c == '/'){
				more_runs = addRunAt(kBBLMRunIsSingleString,r,bblm_callbacks,-2);
				tag = true;
			}
			else if( c == '!' ){
				if (nextchar(r, p, pb, chars) == '-'){
					if (nextchar(r, p, pb, chars) == '-'){
						more_runs = addRunAt(kBBLMRunIsSingleString,r,bblm_callbacks,-2);
						++htmlcomment;
					}	
				}
				else{
					tag = true;	
					more_runs = addRunAt(kBBLMRunIsSingleString,r,bblm_callbacks,-2);
				}
			}
			else if( c == '?' ){
				more_runs = addRunAt(kBBLMRunIsSingleString,r,bblm_callbacks,-2);
				xmlprocdir = true;
			}
		}
		else if (tripple  && xmlprocdir && c == '>'){
			more_runs = addRunAt(kPyBBLMStringProcDir,r,bblm_callbacks,0);
			xmlprocdir = false;
		}
		else if (tripple  && htmlcomment && chars[1] == '-' && c == '>'){
			if (--htmlcomment == 0){
				more_runs = addRunAt(kPyBBLMStringComment,r,bblm_callbacks,0);
			}
		}
		else if (tag){
			if (chars[1] == '=' && c == '"'){
				more_runs = addRunAt(kPyBBLMStringTagAttr,r,bblm_callbacks,-1);
				qtagval = true;
			}
			else if ( chars[1] == '=' && isalnum(c)){
				more_runs = addRunAt(kPyBBLMStringTagAttr,r,bblm_callbacks,-1);
				wtagval = true;
			}
			else if (c=='>' && !qtagval){
				if (wtagval){
					more_runs = addRunAt(kPyBBLMStringTagVal,r,bblm_callbacks,-1);
					wtagval = false;
				}
				more_runs = addRunAt(kPyBBLMStringTag,r,bblm_callbacks);
				tag = false;
			}
			else if (wtagval && isspace(c)){
				more_runs = addRunAt(kPyBBLMStringTagVal,r,bblm_callbacks);
				wtagval = false;
			}
			else if (qtagval && c=='"'){
				more_runs = addRunAt(kPyBBLMStringTagVal,r,bblm_callbacks);
				qtagval = false;
			}
			else if (!qtagval && isspace(c)){
				more_runs = addRunAt(kPyBBLMStringTag,r,bblm_callbacks);
			}
			
		}
		
#endif		
		c = nextchar(r, p, pb,chars);
		
	}
	return more_runs && addRunAt(kBBLMRunIsSingleString,r,bblm_callbacks);
}

static bool colorcomment(BBLMParamBlock &pb,
				struct runloc &r,
				BBLMTextIterator &p,
				const BBLMCallbackBlock &bblm_callbacks)
{

	while (UniChar c = nextchar(r,p, pb,NULL)){
		if (c=='\r'|| c=='\n'){
			break;
		}
	}
	return addRunAt(kBBLMRunIsLineComment,r,bblm_callbacks);
}

static void CalculateRuns(BBLMParamBlock &pb,
			const BBLMCallbackBlock &bblm_callbacks)

{
	bool more_runs=true;
	UniChar c;	
	struct runloc r;
	BBLMTextIterator p(pb);
	
	charbuf chars;
	c = start(r, p, pb,chars);
	
	while (c && more_runs){
		// Process a char

		//If we're in the basic 'code' state, check for each interesting char (rundelims[i].start).
		switch (c){
		case '\'':
		case '"': 
			more_runs = addRunAt(kBBLMRunIsCode,r,bblm_callbacks,(chars[1]=='r'||chars[1]=='R'?-2:-1) );
			if (more_runs){
				more_runs = colorstr(c,pb,r,p,bblm_callbacks);
			}
			break;
		case '#' :
			more_runs = addRunAt(kBBLMRunIsCode,r,bblm_callbacks,-1);
			if (more_runs){
				more_runs = colorcomment(pb,r,p,bblm_callbacks); 
			}
			break;
		default:
			break;
		}
		c = nextchar(r, p, pb,chars);
	}
	if (more_runs){
		addRunAt(kBBLMRunIsCode,r,bblm_callbacks,-1);
	}
	

}

static bool isStringKind(BBLMRunCode kind)
{
	return (kind==kPyBBLMStringSubst||
			kind==kBBLMRunIsSingleString||
			kind==kPyBBLMStringTag||
			kind==kPyBBLMStringTagAttr||
			kind==kPyBBLMStringTagVal||
			kind==kPyBBLMStringProcDir|| 
			kind==kPyBBLMStringComment);
}

static void AdjustRange(BBLMParamBlock &params,
						const BBLMCallbackBlock &callbacks)
{	
	DescType language;
	BBLMRunCode kind;
	SInt32 charPos;
	SInt32 length;
	UInt32 index = params.fAdjustRangeParams.fStartIndex;
	
	while(	index > 0 &&
			bblmGetRun(&callbacks, index, language, kind, charPos, length) &&
			isStringKind(kind)){
	 	index--;
	};
	 params.fAdjustRangeParams.fStartIndex = index;
}


// The next couple funcs process the text of a file assumming it's in 1 piece in memory,
// so they may not be called from CalculateRuns.

static bool matchword(BBLMParamBlock &pb, const char *pat ,unsigned long *pos)
{
	BBLMTextIterator	asciText(pb);

	int i;
	for (i=0; pat[i]; i++){
		if (*pos+i>=pb.fTextLength){
			return false;
		}
		if (asciText[*pos+i] != pat[i]){
			return false;
		}
	}
	if ((*pos+i<pb.fTextLength)&&iswordchar(asciText[*pos+i])){
		return false;
	}
	*pos+=i;
	return true;
}	

static int matchindent(BBLMParamBlock &pb, UInt32 *pos)
{	
	BBLMTextIterator	asciText(pb);

	int indent=0;
		
	while(*pos<pb.fTextLength){
		switch (/*(char)(pb.fTextIsUnicode?uniText[pos]:*/asciText[*pos]/*)*/){
		case ' ':
			++*pos;
			indent++;
			break;	
		case '\t':
			++*pos;		
			indent+=8;
			break;
		case '#':
			return -1;
			break;
		default:
			return indent;
			break;
		}
	}
	return indent;	
}


static void eat3str(char delim,BBLMParamBlock &pb,unsigned long* pos)
{
	BBLMTextIterator	asciText(pb);

	while (*pos<pb.fTextLength){ 
		if (asciText[*pos] == asciText[*pos-1] == asciText[*pos-2] == delim && (asciText[*pos-3] != '\\' || asciText[*pos-4] == '\\')){
			break;
		}
		++*pos;
	}	
}

static void eat_line(BBLMParamBlock &pb, unsigned long* pos)
{
	BBLMTextIterator	asciText(pb);

	while (*pos<pb.fTextLength){
		char c = asciText[(*pos)++];
		if (c=='\r' || c=='\n'){
			break;
		}
		else if  (*pos+6<pb.fTextLength && asciText[*pos] == asciText[*pos+1] == asciText[*pos+2] == '"'){
			*pos+=6;
			eat3str('"', pb, pos);
		}
		else if  (*pos+6<pb.fTextLength && asciText[*pos] == asciText[*pos+1] == asciText[*pos+2] == '\''){
			*pos+=6;
			eat3str('\'', pb, pos);
		}
	}
	while ((asciText[*pos]=='\r' || asciText[*pos]=='\n') && *pos<pb.fTextLength) {++*pos;}

}

static void addItem(BBLMProcInfo *procInfo, UInt32 pos, int nest, BBLMFunctionKinds kind, BBLMParamBlock &pb, const BBLMCallbackBlock *bblm_callbacks)
{
	UInt32 funcnamelen=0;
	UInt32 offset=0;
	BBLMTextIterator	asciText(pb);
	OSErr err;

	UInt32 funcstart = pos - (kind == kBBLMFunctionMark ? 3/*strlen("def")*/ : 5/*strlen("class")*/);
	while (funcstart>0 && isspace(asciText[funcstart-1]) && asciText[funcstart-1] != 0x0D) {funcstart--;}
	
	while (isspace(asciText[pos]) && pos<pb.fTextLength) {++pos;}
	UInt32 fnamestart = pos;
	while ((isalnum(asciText[pos])||asciText[pos]=='_') && pos<pb.fTextLength) {pos++; funcnamelen++;}
	
	BBLMTextIterator	nameStart(asciText, fnamestart);
	
	err = bblmAddTokenToBuffer(	bblm_callbacks, 
								pb.fFcnParams.fTokenBuffer,
								nameStart.Address(),
								funcnamelen,
								pb.fTextIsUnicode,
								&offset);
	
	procInfo[nest].fFunctionStart = funcstart;	//	char offset in file of first character of function
	//procInfo[nest].fFunctionEnd = pos;	//	char offset of last character of function
	
	procInfo[nest].fSelStart = fnamestart;		//	first character to select when choosing function
	procInfo[nest].fSelEnd = pos;		//	last character to select when choosing function
	
	procInfo[nest].fFirstChar = fnamestart;		//	first character to make visible when choosing function
	
	procInfo[nest].fKind = kind;
	
	procInfo[nest].fIndentLevel = nest;	//	indentation level of token
	procInfo[nest].fFlags = 0;			//	token flags (see BBLMFunctionFlags)
	procInfo[nest].fNameStart = offset;		//	char offset in token buffer of token name
	procInfo[nest].fNameLength = funcnamelen;	//	length of token name
																
}

static void commitfunc(BBLMProcInfo &proc, unsigned int funcEnd, BBLMParamBlock &pb, const BBLMCallbackBlock *bblm_callbacks)
{
	BBLMTextIterator textIter(pb);

	// walk backwards over all the intervening blank lines
	while (funcEnd > 0 && isspace(textIter[funcEnd]))
		funcEnd--;
		
	// walk forwards to the end of the last line in the function
	while (funcEnd < pb.fTextLength && textIter[funcEnd] != 0x0D)
		funcEnd++;

	UInt32 index = 0;
	proc.fFunctionEnd = funcEnd + 1;
	(void) bblmAddFunctionToList(bblm_callbacks, pb.fFcnParams.fFcnList, proc, &index);
	
	UInt32 rangeStart = proc.fSelStart;
	int lastNonWSChar = 0;
	
	// walk forwards to the end of the line (this may be several lines if they
	// were continued, so we have to look for the ":" function terminator
	while (rangeStart < pb.fTextLength)
	{
		if (! isspace(textIter[rangeStart]))
			lastNonWSChar = textIter[rangeStart];
		
		if (textIter[rangeStart] == 0x0D)
		{
			if (lastNonWSChar == ':')
				break;
			lastNonWSChar = 0;	
		}
			
		rangeStart++;
	}
		
	// back up over any trailing whitespace on the 
	while (rangeStart > 0 && isspace(textIter[rangeStart-1]))
		rangeStart--;

	if (funcEnd > rangeStart)
		(void) bblmAddFoldRange(bblm_callbacks, rangeStart, funcEnd - rangeStart, (proc.fKind == kBBLMFunctionMark ? kBBLMFunctionAutoFold : kBBLMClassAutoFold));
}

enum {
	MAX_NEST = 10
};

static void ScanForFunctions(BBLMParamBlock &pb,
			const BBLMCallbackBlock &bblm_callbacks)
{
	int indents[MAX_NEST]= {0};
	BBLMProcInfo funcs[MAX_NEST]; 
	int nest = 0;
	
	UInt32 pos=0; // current character offset

	while (pos<pb.fTextLength){
		
		int start_line = pos;
		int indent = matchindent(pb, &pos);
		
		if (indent >= 0){
			// When the indent level is back to what is was when we enecountered the 'def' the function is over. 
			while(nest > 0 && indent <= indents[nest-1]){ 
				nest--;
				commitfunc(funcs[nest], start_line-1, pb, &bblm_callbacks);	
			}
			
			if (nest < MAX_NEST){
				if (matchword(pb,"def", &pos)){
					addItem(funcs, pos, nest, kBBLMFunctionMark, pb, &bblm_callbacks);
					indents[nest] = indent;
					nest++;
				}
				else if (matchword(pb, "class", &pos)){
					addItem(funcs, pos, nest, kBBLMTypedef, pb, &bblm_callbacks);
					indents[nest] = indent;					
					nest++;
				}
			}
		}
		eat_line(pb,&pos);
	}

	while(nest > 0){ 
		commitfunc(funcs[--nest], pos, pb, &bblm_callbacks);
	}
	
}

#pragma mark -

//	JFH 040126 Copied from std_guessers.cp
//
//	RMS 011017 in order to avoid performance problems that can occur
//	when trying to guess in huge files, establish a maximum length
//	of text that we're going to guess at, and each of our guessers
//	will pin to that length. 64K is fine for the cases that we deal
//	with.
//

enum
{
	kMaxGuessTextLen	=	65536L
};

//	JFH 040126 Copied from app_util.h

inline	long	pin(const UInt32 a, const UInt32 b)	{ return (a > b ? b : a); }


static	bool	text_has_shebang(BBLMParamBlock &params, const BBXTCallbackBlock *bbxt_callbacks)
{		
	const	UInt8	shebang[] = "#!\\s*/.*python(\\d+\\.\\d+)*";	//	note double backslashes for the compiler
			long	match_start;
			long	match_end;
			long	cntx = 0;
			SInt16	re_error;
			bool	result = false;
							
	if (noErr == bbxtXPrepareGrep(bbxt_callbacks, true, shebang, false, &re_error, &cntx) &&
		noErr == bbxtXGrepSearchPtr(bbxt_callbacks,
									cntx, false, false,
									params.fText, params.fTextIsUnicode,
									0, pin(params.fTextLength, kMaxGuessTextLen),
									&match_start, &match_end))
	{
		//
		//	We have to use a CharIterator to help out, because of the
		//	higgledy-piggledy Unicode stuff...
		//
		
		if (match_start == 0)	//	RMS 010928 must be at BOF
		{
			//
			//	make sure that there's whitespace after the command,
			//	or else that we're at EOF (which would be odd, but not unheard-of.
			//

			if ((UInt32)match_end < params.fTextLength)
			{
				BBLMTextIterator	p(params);
				
				p += match_end;
				
				//	make sure there's whitespace after the command
				result = isspace(*p);
			}
			else
			{
				//	pattern matched at EOF. Odd, but it's enough for us.
				
				result = true;
			}
		}
	}
	
	bbxtDoneGrep(bbxt_callbacks, cntx);
	return result;
}

static	OSErr	GuessLanguage(BBLMParamBlock &params, const BBXTCallbackBlock *bbxt_callbacks)
{
	if (text_has_shebang(params, bbxt_callbacks))
		params.fGuessLanguageParams.fGuessResult = kBBLMGuessDefiniteYes;
	
	return noErr;
}

#pragma mark -

extern "C"
{

OSErr PythonMachO(BBLMParamBlock &params,
			const BBLMCallbackBlock &bblm_callbacks,
			const BBXTCallbackBlock &bbxt_callbacks)
{
	OSErr result;

	if ((params.fSignature != kBBLMParamBlockSignature) ||
		(params.fVersion < kBBLMParamBlockVersion))
	{
		return paramErr;
	}
	
	switch (params.fMessage)
	{
		case kBBLMInitMessage:
		case kBBLMDisposeMessage:
		{
			result = noErr;	// nothing to do
			break;
		}
		
		case kBBLMCalculateRunsMessage:
			CalculateRuns(params, bblm_callbacks);
			result = noErr;
			break;

		case kBBLMScanForFunctionsMessage:
			ScanForFunctions(params, bblm_callbacks);
			result = noErr;
			break;

		case kBBLMAdjustRangeMessage:
			AdjustRange(params, bblm_callbacks);
			result = noErr;
			break;
		
		case kBBLMMapRunKindToColorCodeMessage:
			switch (params.fMapRunParams.fRunKind){
			case kPyBBLMStringSubst:
				params.fMapRunParams.fColorCode = kBBLMSGMLAttributeNameColor;
				params.fMapRunParams.fMapped =	true;
				break;
			case kPyBBLMStringTag:
				params.fMapRunParams.fColorCode = kBBLMSGMLTagColor;
				params.fMapRunParams.fMapped =	true;
				break;
			case kPyBBLMStringTagAttr:
				params.fMapRunParams.fColorCode = kBBLMSGMLAttributeNameColor;
				params.fMapRunParams.fMapped =	true;
				break;
			case kPyBBLMStringTagVal:
				params.fMapRunParams.fColorCode = kBBLMSGMLAttributeValueColor;
				params.fMapRunParams.fMapped =	true;
				break;
			case  kPyBBLMStringProcDir:
				params.fMapRunParams.fColorCode = kBBLMXMLProcessingInstructionColor;
				params.fMapRunParams.fMapped =	true;			
				break;
			case  kPyBBLMStringComment:
				params.fMapRunParams.fColorCode = kBBLMCommentColor;
				params.fMapRunParams.fMapped =	true;			
				break;
			default:
				params.fMapRunParams.fMapped =	false;
			}
			result = noErr;
			break;
		case kBBLMMapColorCodeToColorMessage:
		/*	if (params.fMapColorParams.fColorCode == kBBLMFirstUserColorCode){
				params.fMapColorParams.fRGBColor = {0x7E67, 0,0};
				params.fMapColorParams.fMapped = true;
			}	*/
			//else{
				params.fMapColorParams.fMapped = false;
			//}	
			result = noErr;
			break;

		case kBBLMEscapeStringMessage:
		case kBBLMAdjustEndMessage:
		case kBBLMSetCategoriesMessage:
		case kBBLMMatchKeywordMessage:
		{
			result = userCanceledErr;
			break;
		}
		
		case kBBLMGuessLanguageMessage:
		{
			result = GuessLanguage(params, &bbxt_callbacks);
			break;
		}
		
		default:
		{
			result = paramErr;
			break;
		}
	}
	return result;	
}

}
