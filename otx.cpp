
#include <Carbon/Carbon.h>

#include <sys/stat.h>

#include <BBLMInterface.h>
#include <BBXTInterface.h>
#include <BBLMTextIterator.h>

#define isEOL(c) (((c) == '\r') || ((c) == '\n'))

static void otx_eatLine(BBLMParamBlock &pb, unsigned long* pos)
{
	BBLMTextIterator text(pb);
	
	while (*pos < pb.fTextLength)
	{
		char c = text[(*pos)++];
		if (isEOL(c))
		{
			break;
		}
	}
	while ((isEOL(text[*pos])) && *pos < pb.fTextLength) {++*pos;}
}

static void otx_scanForFunctions(BBLMParamBlock &pb, const BBLMCallbackBlock &bblm_callbacks)
{
	UInt32 pos = 0;
	BBLMProcInfo procInfo = {0};
	
	while (pos < pb.fTextLength)
	{
		BBLMTextIterator text(pb);
		
		bool isClassMethod = text[pos] == '+';
		bool isInstanceMethod = text[pos] == '-';
		
		if ((text[pos] != ' ' && text[pos] != '\t' && text[pos] != '/' && text[pos] != '(' && text[pos] != '#') && !(isClassMethod && isdigit(text[pos+1])))
		{
			if (procInfo.fFunctionStart != 0)
			{
				// we know the end of the function once we have found the next function
				procInfo.fFunctionEnd = pos - 1;
				bblmAddFunctionToList(&bblm_callbacks, pb.fFcnParams.fFcnList, procInfo, NULL);
			}
			
			UInt32 offset = 0;
			UInt32 funcnamelen = 0;
			UInt32 fselstart = pos;
			UInt32 fnamestart = pos;
			
			if (isClassMethod || isInstanceMethod)
			{
				// ObjC method
				fnamestart++;
				while (text[pos] != ')') { pos++; fnamestart++; }
				while (!isEOL(text[pos])) { pos++; funcnamelen++; }				
			}
			else
			{
				// function
				if (text[pos] == '_') { fnamestart++; funcnamelen--; }
				while (!isEOL(text[pos])) { pos++; funcnamelen++; }
				if (text[pos-1] == ':') { funcnamelen--; }
			}
			
			BBLMTextIterator nameStart(text, fnamestart);
			
			bblmAddTokenToBuffer(&bblm_callbacks, pb.fFcnParams.fTokenBuffer, nameStart.Address(), funcnamelen, pb.fTextIsUnicode, &offset);
			
			procInfo.fFunctionStart = fselstart;     // char offset in file of first character of function
			
			procInfo.fSelStart = fselstart;          // first character to select when choosing function
			procInfo.fSelEnd = pos;                  // last character to select when choosing function
			
			procInfo.fFirstChar = fnamestart;        // first character to make visible when choosing function
			
			procInfo.fKind = isClassMethod ? kBBLMPragmaMark : (isInstanceMethod ? kBBLMTypedef : kBBLMFunctionMark);
			
			procInfo.fIndentLevel = 0;               // indentation level of token
			procInfo.fFlags = 0;                     // token flags (see BBLMFunctionFlags)
			procInfo.fNameStart = offset;            // char offset in token buffer of token name
			procInfo.fNameLength = funcnamelen;      // length of token name			
		}
		otx_eatLine(pb, &pos);
	}
	procInfo.fFunctionEnd = pos;
	bblmAddFunctionToList(&bblm_callbacks, pb.fFcnParams.fFcnList, procInfo, NULL);
}

static void otx_guessLanguage(BBLMParamBlock &pb, const BBXTCallbackBlock *bbxt_callbacks)
{
	bool isExecutable = false;
	
	CFMutableStringRef path = CFStringCreateMutable(kCFAllocatorDefault, PATH_MAX);
	char sysPath[PATH_MAX];
	UInt32 pos = 0;
	BBLMTextIterator text(pb);
	
	while (!isEOL(text[pos]))
	{
		UniChar c = text[pos++];
		CFStringAppendCharacters(path, &c, 1);
	}
	CFStringTrim(path, CFSTR(":"));
	
	CFStringGetFileSystemRepresentation(path, sysPath, sizeof(sysPath));
	struct stat st;
	stat(sysPath, &st);
	if ((st.st_mode & S_IXUSR) == S_IXUSR)
	{
		isExecutable = true;
	}
	
	CFRelease(path);
	
	pb.fGuessLanguageParams.fGuessResult = isExecutable ? kBBLMGuessDefiniteYes : kBBLMGuessDefiniteNo;
}

extern "C" OSErr otx_main(BBLMParamBlock &params, const BBLMCallbackBlock &bblm_callbacks, const BBXTCallbackBlock &bbxt_callbacks)
{
	OSErr result;
	
	if ((params.fSignature != kBBLMParamBlockSignature) || (params.fVersion < kBBLMParamBlockVersion))
	{
		return paramErr;
	}

	switch (params.fMessage)
	{
		case kBBLMInitMessage:
		case kBBLMDisposeMessage:
		{
			result = noErr;
			break;
		}
		
		case kBBLMScanForFunctionsMessage:
		{
			otx_scanForFunctions(params, bblm_callbacks);
			result = noErr;
			break;
		}
		
		case kBBLMGuessLanguageMessage:
		{
			otx_guessLanguage(params, &bbxt_callbacks);
			result = noErr;
			break;
		}
		
		case kBBLMCalculateRunsMessage:
		{
			result = noErr;
			break;
		}
		
		case kBBLMAdjustRangeMessage:
		{
			result = noErr;
			break;
		}
		
		case kBBLMMapRunKindToColorCodeMessage:
		{
			params.fMapRunParams.fMapped =	false;
			result = noErr;
			break;
		}
		
		case kBBLMMapColorCodeToColorMessage:
		{
			params.fMapColorParams.fMapped = false;
			result = noErr;
			break;
		}
		
		case kBBLMEscapeStringMessage:
		case kBBLMAdjustEndMessage:
		case kBBLMSetCategoriesMessage:
		case kBBLMMatchKeywordMessage:
		{
			result = userCanceledErr;
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
