//	-*- tab-width: 2; -*-

#include "TeXModule.h"

static SInt32			origIndex;
static BBLMRunRec	origRun;

static void AdjustRange(			BBLMParamBlock    &params,
												const BBLMCallbackBlock& /*callbacks*/,
												const BBXTCallbackBlock& /*bbxt_callbacks*/)
{
	origIndex = params.fAdjustRangeParams.fOrigStartIndex;
	
	if (origIndex >= 0)
		origRun = params.fAdjustRangeParams.fOrigStartRun;
}

static void AdjustEnd(			BBLMParamBlock    &params,
											const BBLMCallbackBlock &callbacks,
											const BBXTCallbackBlock &bbxt_callbacks)
{
	SInt32 runCount = bblmRunCount(&callbacks);
	
	if (origIndex >= 0 && origIndex < runCount)
	{
		BBLMRunRec s;
		
		bblmGetRun(&callbacks, origIndex, s.language, s.kind, s.startPos, s.length);
		
		if (origRun.language == s.language	&&
				origRun.kind     == s.kind			&&
				origRun.startPos == s.startPos	&&
				origRun.length   == s.length)
			{
				long	sel_start, sel_end;
				long	first_char;
				
				bbxtGetSelection((&bbxt_callbacks), &sel_start, &sel_end, &first_char);
				
				params.fAdjustEndParams.fEndOffset = sel_end;
			}
	}
}

static void match_keyword(BBLMParamBlock &params)
{
	if (kBBLMMatchKeywordMessage == params.fMessage)
	{
		SInt8* token = params.fMatchKeywordParams.fToken;
		
		params.fMatchKeywordParams.fKeywordMatched =
			(params.fMatchKeywordParams.fTokenLength > 1 && token[0] == '\\' && !isdigit(token[1]));
	}
	else if (kBBLMMatchKeywordWithCFStringMessage == params.fMessage)
	{
		params.fMatchKeywordWithCFStringParams.fKeywordMatched =
			((NULL != params.fMatchKeywordWithCFStringParams.fToken) &&
				(CFStringGetLength(params.fMatchKeywordWithCFStringParams.fToken) > 1) &&
				('\\' == CFStringGetCharacterAtIndex(params.fMatchKeywordWithCFStringParams.fToken, 0)) &&
				(! isdigit(CFStringGetCharacterAtIndex(params.fMatchKeywordWithCFStringParams.fToken, 1))));
	}
}

static	void	CanSpellCheckRun(BBLMParamBlock &params)
{
	params.fCanSpellCheckRunParams.fRunCanBeSpellChecked =
		(kBBLMRunIsLineComment != params.fCanSpellCheckRunParams.fRunKind);
}

static void escape_char(BBLMParamBlock &params)
{
	bblmEscCharParams&	esc = params.fEscapeCharParams;
	UInt8*							s = esc.fOutputString;
	UInt16							i = ( params.fTextIsUnicode
															? *(UInt16*)(params.fText)
															: *(UInt8*) (params.fText) );
	
	static const UInt8 hex[] = "0123456789ABCDEF";
	
	if ( i > 0xFF )
	{
		*s++ = 5;
		*s   = '$';
		s   += 4;
	}	
	else
	{
		*s++ = 3;
		*s   = '$';
		s   += 2;
	}
	
	while (s > (esc.fOutputString + 1))
	{
		*s-- = hex[i & 0x000F];
		i >>= 4;
	}
	
	esc.fOutputStringSize = *s;
}

extern "C"
{

OSErr TeXMachO(BBLMParamBlock		&params,
						const BBLMCallbackBlock	&bblm_callbacks,
						const BBXTCallbackBlock	&bbxt_callbacks)
{
	OSErr	err = noErr;
	
	//
	//	a language module must always make sure that the parameter block
	//	is valid by checking the signature, version number, and size
	//	of the parameter block. Note also that version 2 is the first
	//	usable version of the parameter block; anything older should
	//	be rejected.
	//

	//
	//	RMS 010925 the check for params.fVersion > kBBLMParamBlockVersion
	//	is overly strict, since there are no structural changes that would
	//	break backward compatibility; only new members are added.
	//
		
	if ((params.fSignature != kBBLMParamBlockSignature) ||
			(params.fVersion == 0)													||
			(params.fVersion < 2)														||
			(params.fLength < sizeof(BBLMParamBlock)))
	{
		return paramErr;
	}
	
	switch (params.fMessage)
	{
		case kBBLMInitMessage:
		case kBBLMDisposeMessage:
			break;
		
		case kBBLMScanForFunctionsMessage:
			ParseTeX(params, bblm_callbacks);
			break;
			
		case kBBLMAdjustRangeMessage:
			
			AdjustRange(params, bblm_callbacks, bbxt_callbacks);
			break;
		
		case kBBLMCalculateRunsMessage:
		{
			bool		colorMathStrings = false;
			Boolean	keyExists = false;
			
			colorMathStrings = CFPreferencesGetAppBooleanValue(CFSTR("LanguageSpecific:ColorMathStrings:TeX"),
																kCFPreferencesCurrentApplication,
																&keyExists);
			if ((! colorMathStrings) && (! keyExists))
			{
				//	try the old setting.
				
				colorMathStrings = CFPreferencesGetAppBooleanValue(CFSTR("TeXLanguageModule:ColorMathStrings"),
																	kCFPreferencesCurrentApplication,
																	NULL);
			}
			
			scan_tex(params,
						bblm_callbacks,
						bbxt_callbacks,
						colorMathStrings);
						
			break;
		}
		
		case kBBLMAdjustEndMessage:
			
			AdjustEnd(params, bblm_callbacks, bbxt_callbacks);
			break;
			
		case kBBLMMapColorCodeToColorMessage:
		case kBBLMMapRunKindToColorCodeMessage:
			
			break;
			
		case kBBLMSetCategoriesMessage:
		{
			SInt8*	cat = params.fCategoryParams.fCategoryTable;
			
			cat['\\'] = '<';
			break;
		}
		
		case kBBLMMatchKeywordMessage:							//	implemented because old versions will send it
		case kBBLMMatchKeywordWithCFStringMessage:
		{
			match_keyword(params);
			break;
		}
		
		case kBBLMEscapeStringMessage:
			
			escape_char(params);
			break;
		
		case kBBLMCanSpellCheckRunMessage:
			CanSpellCheckRun(params);
			break;
			
		default:
			
			err = paramErr;
			break;
	}
	
	return err;
}
}
