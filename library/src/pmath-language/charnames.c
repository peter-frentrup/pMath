#include <pmath-language/charnames.h>

#include <pmath-util/debug.h>
#include <pmath-util/hashtables.h>
#include <pmath-util/incremental-hash-private.h>

#include <pmath-language/tokens.h>

#include <string.h>


struct named_char_t {
  uint32_t unichar;
  const char *name;
};

#define NAMED_CHAR_ARR_COUNT  (sizeof(named_char_array)/sizeof(named_char_array[0]))
static struct named_char_t named_char_array[] = {

  {0x1D7D8, "DoubleStruckZero"},
  {0x1D7D9, "DoubleStruckOne"},
  {0x1D7DA, "DoubleStruckTwo"},
  {0x1D7DB, "DoubleStruckThree"},
  {0x1D7DC, "DoubleStruckFour"},
  {0x1D7DD, "DoubleStruckFive"},
  {0x1D7DE, "DoubleStruckSix"},
  {0x1D7DF, "DoubleStruckSeven"},
  {0x1D7E0, "DoubleStruckEight"},
  {0x1D7E1, "DoubleStruckNine"},
  
  {0x1D552, "DoubleStruckA"},
  {0x1D553, "DoubleStruckB"},
  {0x1D554, "DoubleStruckC"},
  {0x1D555, "DoubleStruckD"},
  {0x1D556, "DoubleStruckE"},
  {0x1D557, "DoubleStruckF"},
  {0x1D558, "DoubleStruckG"},
  {0x1D559, "DoubleStruckH"},
  {0x1D55A, "DoubleStruckI"},
  {0x1D55B, "DoubleStruckJ"},
  {0x1D55C, "DoubleStruckK"},
  {0x1D55D, "DoubleStruckL"},
  {0x1D55E, "DoubleStruckM"},
  {0x1D55F, "DoubleStruckN"},
  {0x1D560, "DoubleStruckO"},
  {0x1D561, "DoubleStruckP"},
  {0x1D562, "DoubleStruckQ"},
  {0x1D563, "DoubleStruckR"},
  {0x1D564, "DoubleStruckS"},
  {0x1D565, "DoubleStruckT"},
  {0x1D566, "DoubleStruckU"},
  {0x1D567, "DoubleStruckV"},
  {0x1D568, "DoubleStruckW"},
  {0x1D569, "DoubleStruckX"},
  {0x1D56A, "DoubleStruckY"},
  {0x1D56B, "DoubleStruckZ"},
  
  {0x1D538, "DoubleStruckCapitalA"},
  {0x1D539, "DoubleStruckCapitalB"},
  { 0x2102, "DoubleStruckCapitalC"},
  {0x1D53B, "DoubleStruckCapitalD"},
  {0x1D53C, "DoubleStruckCapitalE"},
  {0x1D53D, "DoubleStruckCapitalF"},
  {0x1D53E, "DoubleStruckCapitalG"},
  { 0x210D, "DoubleStruckCapitalH"},
  {0x1D540, "DoubleStruckCapitalI"},
  {0x1D541, "DoubleStruckCapitalJ"},
  {0x1D542, "DoubleStruckCapitalK"},
  {0x1D543, "DoubleStruckCapitalL"},
  {0x1D544, "DoubleStruckCapitalM"},
  { 0x2115, "DoubleStruckCapitalN"},
  {0x1D546, "DoubleStruckCapitalO"},
  { 0x2119, "DoubleStruckCapitalP"},
  { 0x211A, "DoubleStruckCapitalQ"},
  { 0x211D, "DoubleStruckCapitalR"},
  {0x1D54A, "DoubleStruckCapitalS"},
  {0x1D54B, "DoubleStruckCapitalT"},
  {0x1D54C, "DoubleStruckCapitalU"},
  {0x1D54D, "DoubleStruckCapitalV"},
  {0x1D54E, "DoubleStruckCapitalW"},
  {0x1D54F, "DoubleStruckCapitalX"},
  {0x1D550, "DoubleStruckCapitalY"},
  { 0x2124, "DoubleStruckCapitalZ"},
  
  {0x1D4B6, "ScriptA"},
  {0x1D4B7, "ScriptB"},
  {0x1D4B8, "ScriptC"},
  {0x1D4B9, "ScriptD"},
  { 0x212F, "ScriptE"},
  {0x1D4BB, "ScriptF"},
  { 0x210A, "ScriptG"},
  {0x1D4BD, "ScriptH"},
  {0x1D4BE, "ScriptI"},
  {0x1D4BF, "ScriptJ"},
  {0x1D4C0, "ScriptK"},
  { 0x2113, "ScriptL"},
  {0x1D4C2, "ScriptM"},
  {0x1D4C3, "ScriptN"},
  { 0x2134, "ScriptO"},
  {0x1D4C5, "ScriptP"},
  {0x1D4C6, "ScriptQ"},
  {0x1D4C7, "ScriptR"},
  {0x1D4C8, "ScriptS"},
  {0x1D4C9, "ScriptT"},
  {0x1D4CA, "ScriptU"},
  {0x1D4CB, "ScriptV"},
  {0x1D4CC, "ScriptW"},
  {0x1D4CD, "ScriptX"},
  {0x1D4CE, "ScriptY"},
  {0x1D4CF, "ScriptZ"},
  
  {0x1D49C, "ScriptCapitalA"},
  { 0x212C, "ScriptCapitalB"},
  {0x1D49E, "ScriptCapitalC"},
  {0x1D49F, "ScriptCapitalD"},
  { 0x2130, "ScriptCapitalE"},
  { 0x2131, "ScriptCapitalF"},
  {0x1D4A2, "ScriptCapitalG"},
  { 0x210B, "ScriptCapitalH"},
  { 0x2110, "ScriptCapitalI"},
  {0x1D4A5, "ScriptCapitalJ"},
  {0x1D4A6, "ScriptCapitalK"},
  { 0x2112, "ScriptCapitalL"},
  { 0x2133, "ScriptCapitalM"},
  {0x1D4A9, "ScriptCapitalN"},
  {0x1D4AA, "ScriptCapitalO"},
  { 0x2118, "ScriptCapitalP"},
  { 0x211B, "ScriptCapitalQ"},
  {0x1D4AD, "ScriptCapitalR"},
  {0x1D4AE, "ScriptCapitalS"},
  {0x1D4AF, "ScriptCapitalT"},
  {0x1D4B0, "ScriptCapitalU"},
  {0x1D4B1, "ScriptCapitalV"},
  {0x1D4B2, "ScriptCapitalW"},
  {0x1D4B3, "ScriptCapitalX"},
  {0x1D4B4, "ScriptCapitalY"},
  {0x1D4B5, "ScriptCapitalZ"},
  
  {0x1D51E, "GothicA"},
  {0x1D51F, "GothicB"},
  {0x1D520, "GothicC"},
  {0x1D521, "GothicD"},
  {0x1D522, "GothicE"},
  {0x1D523, "GothicF"},
  {0x1D524, "GothicG"},
  {0x1D525, "GothicH"},
  {0x1D526, "GothicI"},
  {0x1D527, "GothicJ"},
  {0x1D528, "GothicK"},
  {0x1D529, "GothicL"},
  {0x1D52A, "GothicM"},
  {0x1D52B, "GothicN"},
  {0x1D52C, "GothicO"},
  {0x1D52D, "GothicP"},
  {0x1D52E, "GothicQ"},
  {0x1D52F, "GothicR"},
  {0x1D530, "GothicS"},
  {0x1D531, "GothicT"},
  {0x1D532, "GothicU"},
  {0x1D533, "GothicV"},
  {0x1D534, "GothicW"},
  {0x1D535, "GothicX"},
  {0x1D536, "GothicY"},
  {0x1D537, "GothicZ"},
  
  {0x1D504, "GothicCapitalA"},
  {0x1D505, "GothicCapitalB"},
  { 0x212D, "GothicCapitalC"},
  {0x1D507, "GothicCapitalD"},
  {0x1D508, "GothicCapitalE"},
  {0x1D509, "GothicCapitalF"},
  {0x1D50A, "GothicCapitalG"},
  { 0x210C, "GothicCapitalH"},
  { 0x2111, "GothicCapitalI"},
  {0x1D50D, "GothicCapitalJ"},
  {0x1D50E, "GothicCapitalK"},
  {0x1D50F, "GothicCapitalL"},
  {0x1D510, "GothicCapitalM"},
  {0x1D511, "GothicCapitalN"},
  {0x1D512, "GothicCapitalO"},
  {0x1D513, "GothicCapitalP"},
  {0x1D514, "GothicCapitalQ"},
  { 0x211C, "GothicCapitalR"},
  {0x1D516, "GothicCapitalS"},
  {0x1D517, "GothicCapitalT"},
  {0x1D518, "GothicCapitalU"},
  {0x1D519, "GothicCapitalV"},
  {0x1D51A, "GothicCapitalW"},
  {0x1D51B, "GothicCapitalX"},
  {0x1D51C, "GothicCapitalY"},
  { 0x2128, "GothicCapitalZ"},
  
  { 0x03B1, "Alpha"},
  { 0x03B2, "Beta"},
  { 0x03B3, "Gamma"},
  { 0x03B4, "Delta"},
  { 0x03F5, "Epsilon"},
  { 0x03B5, "CurlyEpsilon"},
  { 0x03B6, "Zeta"},
  { 0x03B7, "Eta"},
  { 0x03B8, "Theta"},
  { 0x03D1, "CurlyTheta"},
  { 0x03B9, "Iota"},
  { 0x03BA, "Kappa"},
  { 0x03F0, "CurlyKappa"},
  { 0x03BB, "Lambda"},
  { 0x03BC, "Mu"},
  { 0x03BD, "Nu"},
  { 0x03BE, "Xi"},
  { 0x03BF, "Omicron"},
  { 0x03C0, "Pi"},
  { 0x03D6, "CurlyPi"},
  { 0x03C1, "Rho"},
  { 0x03F1, "CurlyRho"},
  { 0x03C2, "FinalSigma"},
  { 0x03C3, "Sigma"},
  { 0x03DB, "Stigma"},
  { 0x03C4, "Tau"},
  { 0x03C5, "Upsilon"},
  { 0x03D5, "Phi"},
  { 0x03C6, "CurlyPhi"},
  { 0x03C7, "Chi"},
  { 0x03C8, "Psi"},
  { 0x03C9, "Omega"},
  
  { 0x0391, "CapitalAlpha"},
  { 0x0392, "CapitalBeta"},
  { 0x0393, "CapitalGamma"},
  { 0x0394, "CapitalDelta"},
  { 0x0395, "CapitalEpsilon"},
  { 0x0396, "CapitalZeta"},
  { 0x0397, "CapitalEta"},
  { 0x0398, "CapitalTheta"},
  { 0x0399, "CapitalIota"},
  { 0x039A, "CapitalKappa"},
  { 0x039B, "CapitalLambda"},
  { 0x039C, "CapitalMu"},
  { 0x039D, "CapitalNu"},
  { 0x039E, "CapitalXi"},
  { 0x039F, "CapitalOmicron"},
  { 0x03A0, "CapitalPi"},
  { 0x03A1, "CapitalRho"},
  { 0x03A3, "CapitalSigma"},
  { 0x03AB, "CapitalStigma"},
  { 0x03A4, "CapitalTau"},
  { 0x03A5, "CapitalUpsilon"},
  { 0x03A6, "CapitalPhi"},
  { 0x03A7, "CapitalChi"},
  { 0x03A8, "CapitalPsi"},
  { 0x03A9, "CapitalOmega"},
  
  { 0x2135, "Aleph"},
  { 0x2136, "Beth"},
  { 0x2137, "Gimel"},
  { 0x2138, "Dalet"},
  
  { 0x00A0, "NonBreakingSpace"},
  { 0x2005, "ThickSpace"},
  { 0x205F, "MediumSpace"},
  { 0x2009, "ThinSpace"},
  { 0x200A, "VeryThinSpace"},
  { 0x200B, "InvisibleSpace"},
  { 0x2060, "NoBreak"},
  { 0x2061, "InvisibleApply"},
  { 0x2062, "InvisibleTimes"},
  { 0x2063, "InvisibleComma"},
  { 0x2064, "InvisiblePlus"},
  
  { 0x0009, "RawTab"},
  { 0x000A, "RawNewline"},
  { 0x000D, "RawReturn"},
  { 0x001B, "RawEscape"},
  { 0x0020, "RawSpace"},
  { 0x0021, "RawExclamation"},
  { 0x0022, "RawDoubleQuote"},
  { 0x0023, "RawNumberSign"},
  { 0x0024, "RawDollar"},
  { 0x0025, "RawPercent"},
  { 0x0026, "RawAmpersand"},
  { 0x0027, "RawQuote"},
  { 0x0028, "RawLeftParenthesis"},
  { 0x0029, "RawRightParenthesis"},
  { 0x002A, "RawStar"},
  { 0x002B, "RawPlus"},
  { 0x002C, "RawComma"},
  { 0x002D, "RawDash"},
  { 0x002E, "RawDot"},
  { 0x002F, "RawSlash"},
  { 0x003A, "RawColon"},
  { 0x003B, "RawSemicolon"},
  { 0x003C, "RawLess"},
  { 0x003D, "RawEqual"},
  { 0x003E, "RawGreater"},
  { 0x003F, "RawQuestion"},
  { 0x0040, "RawAt"},
  { 0x005B, "RawLeftBracket"},
  { 0x005C, "RawBackslash"},
  { 0x005D, "RawRightBracket"},
  { 0x005E, "RawWedge"},
  { 0x005F, "RawUnderscore"},
  { 0x0060, "RawBackquote"},
  { 0x007B, "RawLeftBrace"},
  { 0x007C, "RawVerticalBar"},
  { 0x007D, "RawRightBrace"},
  { 0x007E, "RawTilde"},
  
  { 0x00A1, "DownExclamation"},
  { 0x00A2, "Cent"},
  { 0x00A3, "Sterling"},
  { 0x00A4, "Currency"},
  { 0x00A5, "Yen"},
  { 0x00A7, "Section"},
  { 0x00A9, "Copyright"},
  { 0x00AB, "LeftGuillemet"},
  { 0x00AC, "Not"},
  { 0x00AD, "DiscretionaryHyphen"},
  { 0x00AE, "RegisteredTrademark"},
  { 0x00B0, "Degree"},
  { 0x00B1, "PlusMinus"},
  { 0x00B5, "Micro"},
  { 0x00B6, "Paragraph"},
  { 0x00B7, "CenterDot"},
  { 0x00BB, "RightGuillemet"},
  { 0x00BF, "DownQuestion"},
  { 0x00D7, "Times"},
  { 0x00F7, "Divide"},
  { 0x22C5, "Dot"},
  { 0x2A2F, "Cross"},
  
  { 0x00C0, "CapitalAGrave"},
  { 0x00C1, "CapitalAAcute"},
  { 0x00C2, "CapitalAHat"},
  { 0x00C3, "CapitalATilde"},
  { 0x00C4, "CapitalADoubleDot"},
  { 0x00C5, "CapitalARing"},
  { 0x00C6, "CapitalAE"},
  { 0x00C7, "CapitalCCedilla"},
  { 0x00C8, "CapitalEGrave"},
  { 0x00C9, "CapitalEAcute"},
  { 0x00CA, "CapitalEHat"},
  { 0x00CB, "CapitalEDoubleDot"},
  { 0x00CC, "CapitalIGrave"},
  { 0x00CD, "CapitalIAcute"},
  { 0x00CE, "CapitalIHat"},
  { 0x00CF, "CapitalIDoubleDot"},
  { 0x00D0, "CapitalEth"},
  { 0x00D1, "CapitalNTilde"},
  { 0x00D2, "CapitalOGrave"},
  { 0x00D3, "CapitalOAcute"},
  { 0x00D4, "CapitalOHat"},
  { 0x00D5, "CapitalOTilde"},
  { 0x00D6, "CapitalODoubleDot"},
  { 0x00D8, "CapitalOSlash"},
  { 0x00D9, "CapitalUGrave"},
  { 0x00DA, "CapitalUAcute"},
  { 0x00DB, "CapitalUHat"},
  { 0x00DC, "CapitalUDoubleDot"},
  { 0x00DD, "CapitalYAcute"},
  { 0x00DE, "CapitalThorn"},
  { 0x00DF, "SZ"},
  { 0x00E0, "AGrave"},
  { 0x00E1, "AAcute"},
  { 0x00E2, "AHat"},
  { 0x00E3, "ATilde"},
  { 0x00E4, "ADoubleDot"},
  { 0x00E5, "ARing"},
  { 0x00E6, "AE"},
  { 0x00E7, "CCedilla"},
  { 0x00E8, "EGrave"},
  { 0x00E9, "EAcute"},
  { 0x00EA, "EHat"},
  { 0x00EB, "EDoubleDot"},
  { 0x00EC, "IGrave"},
  { 0x00ED, "IAcute"},
  { 0x00EE, "IHat"},
  { 0x00EF, "IDoubleDot"},
  { 0x00F0, "Eth"},
  { 0x00F1, "NTilde"},
  { 0x00F2, "OGrave"},
  { 0x00F3, "OAcute"},
  { 0x00F4, "OHat"},
  { 0x00F5, "OTilde"},
  { 0x00F6, "ODoubleDot"},
  { 0x00F8, "OSlash"},
  { 0x00F9, "UGrave"},
  { 0x00FA, "UAcute"},
  { 0x00FB, "UHat"},
  { 0x00FC, "UDoubleDot"},
  { 0x00FD, "YAcute"},
  { 0x00FE, "Thorn"},
  { 0x00FF, "YDoubleDot"},
  
  { 0x0100, "CapitalABar"},
  { 0x0101, "ABar"},
  { 0x0102, "CapitalACup"},
  { 0x0103, "ACup"},
  { 0x0104, "CapitalAOgonek"},
  { 0x0105, "AOgonek"},
  { 0x0106, "CapitalCAcute"},
  { 0x0107, "CAcute"},
  { 0x0108, "CapitalCHat"},
  { 0x0109, "CHat"},
  { 0x010A, "CapitalCDot"},
  { 0x010B, "CDot"},
  { 0x010C, "CapitalCHacek"},
  { 0x010D, "CHacek"},
  { 0x010E, "CapitalDHacek"},
  { 0x010F, "DHacek"},
  { 0x0110, "CapitalDStroke"},
  { 0x0111, "DStroke"},
  { 0x0112, "CapitalEBar"},
  { 0x0113, "EBar"},
  { 0x0114, "CapitalECup"},
  { 0x0115, "ECup"},
  { 0x0116, "CapitalEDot"},
  { 0x0117, "EDot"},
  { 0x0118, "CapitalEOgonek"},
  { 0x0119, "EOgonek"},
  { 0x011A, "CapitalEHacek"},
  { 0x011B, "EHacek"},
  { 0x011C, "CapitalGHat"},
  { 0x011D, "GHat"},
  { 0x011E, "CapitalGCup"},
  { 0x011F, "GCup"},
  { 0x0120, "CapitalGDot"},
  { 0x0121, "GDot"},
  { 0x0122, "CapitalGCedilla"},
  { 0x0123, "GCedilla"},
  { 0x0124, "CapitalHHat"},
  { 0x0125, "HHat"},
  { 0x0126, "CapitalHStroke"},
  { 0x0127, "HStroke"},
  { 0x0128, "CapitalITilde"},
  { 0x0129, "ITilde"},
  { 0x012A, "CapitalIBar"},
  { 0x012B, "IBar"},
  { 0x012C, "CapitalICup"},
  { 0x012D, "ICup"},
  { 0x012E, "CapitalIOgonek"},
  { 0x012F, "IOgonek"},
  { 0x0130, "CapitalIDot"},
  { 0x0131, "DotlessI"},
  { 0x0132, "CapitalIJ"},
  { 0x0133, "IJ"},
  { 0x0134, "CapitalJHat"},
  { 0x0135, "JHat"},
  { 0x0136, "CapitalKCedilla"},
  { 0x0137, "KCedilla"},
  { 0x0138, "Kra"},
  { 0x0139, "CapitalLAcute"},
  { 0x013A, "LAcute"},
  { 0x013B, "CapitalLCedilla"},
  { 0x013C, "LCedilla"},
  { 0x013D, "CapitalLHacek"},
  { 0x013E, "LHacek"},
  { 0x013F, "CapitalLMiddleDot"},
  { 0x0140, "LMiddleDot"},
  { 0x0141, "CapitalLSlash"},
  { 0x0142, "LSlash"},
  { 0x0143, "CapitalNAcute"},
  { 0x0144, "NAcute"},
  { 0x0145, "CapitalNCedilla"},
  { 0x0146, "NCedilla"},
  { 0x0147, "CapitalNHacek"},
  { 0x0148, "NHacek"},
  { 0x0149, "NAfterQuote"},
  { 0x014A, "CapitalEng"},
  { 0x014B, "Eng"},
  { 0x014C, "CapitalOBar"},
  { 0x014D, "OBar"},
  { 0x014E, "CapitalOCup"},
  { 0x014F, "OCup"},
  { 0x0150, "CapitalODoubleAcute"},
  { 0x0151, "ODoubleAcute"},
  { 0x0152, "CapitalOE"},
  { 0x0153, "OE"},
  { 0x0154, "CapitalRAcute"},
  { 0x0155, "RAcute"},
  { 0x0156, "CapitalRCedilla"},
  { 0x0157, "RCedilla"},
  { 0x0158, "CapitalRHacek"},
  { 0x0159, "RHacek"},
  { 0x015A, "CapitalSAcute"},
  { 0x015B, "SAcute"},
  { 0x015C, "CapitalSHat"},
  { 0x015D, "SHat"},
  { 0x015E, "CapitalSCedilla"},
  { 0x015F, "SCedilla"},
  { 0x0160, "CapitalSHacek"},
  { 0x0161, "SHacek"},
  { 0x0162, "CapitalTCedilla"},
  { 0x0163, "TCedilla"},
  { 0x0164, "CapitalTHacek"},
  { 0x0165, "THacek"},
  { 0x0166, "CapitalTStroke"},
  { 0x0167, "TStroke"},
  { 0x0168, "CapitalUTilde"},
  { 0x0169, "UTilde"},
  { 0x016A, "CapitalUBar"},
  { 0x016B, "UBar"},
  { 0x016C, "CapitalUCup"},
  { 0x016D, "UCup"},
  { 0x016E, "CapitalURing"},
  { 0x016F, "URing"},
  { 0x0170, "CapitalUDoubleAcute"},
  { 0x0171, "UDoubleAcute"},
  { 0x0172, "CapitalUOgonek"},
  { 0x0173, "UOgonek"},
  { 0x0174, "CapitalWHat"},
  { 0x0175, "WHat"},
  { 0x0176, "CapitalYHat"},
  { 0x0177, "YHat"},
  { 0x0178, "CapitalYDoubleDot"},
  { 0x0179, "CapitalZAcute"},
  { 0x017A, "ZAcute"},
  { 0x017B, "CapitalZDot"},
  { 0x017C, "ZDot"},
  { 0x017D, "CapitalZHacek"},
  { 0x017E, "ZHacek"},
  { 0x017F, "LongS"},
  
  { 0x2010, "Hyphen"},
  { 0x2013, "Dash"},
  { 0x2014, "LongDash"},
  { 0x2018, "HighSixQuote"},
  { 0x2019, "HighNineQuote"},
  { 0x201A, "LowNineQuote"},
  { 0x201B, "HighReversedNineQuote"},
  { 0x201C, "HighSixDoubleQuote"},
  { 0x201D, "HighNineDoubleQuote"},
  { 0x201E, "LowNineDoubleQuote"},
  { 0x201F, "HighReversedNineDoubleQuote"},
  { 0x2020, "Dagger"},
  { 0x2021, "DoubleDagger"},
  { 0x2026, "Ellipsis"},
  { 0x2032, "Prime"},
  { 0x2033, "DoublePrime"},
  { 0x2034, "TriplePrime"},
  { 0x2039, "LeftSingleGuillemet"},
  { 0x203A, "RightSingleGuillemet"},
  { 0x2057, "QuadruplePrime"},
  
  { 0x2145, "CapitalDifferentialD"},
  { 0x2146, "DifferentialD"},
  { 0x2147, "ExponentialE"},
  { 0x2148, "ImaginaryI"},
  { 0x2149, "ImaginaryJ"},
  { 0x2200, "ForAll"},
  { 0x2202, "PartialD"},
  { 0x2203, "Exists"},
  { 0x2204, "NotExists"},
  { 0x2205, "EmptySet"},
  { 0x2207, "Del"},
  { 0x2208, "Element"},
  { 0x2209, "NotElement"},
  { 0x220B, "ReverseElement"},
  { 0x220C, "NotReverseElement"},
  { 0x220D, "SuchThat"},
  { 0x220F, "Product"},
  { 0x2210, "Coproduct"},
  { 0x2211, "Sum"},
  { 0x2213, "MinusPlus"},
  { 0x2216, "Backslash"},
  { 0x2218, "SmallCircle"},
  { 0x221A, "Sqrt"},
  { 0x221D, "Proportional"},
  { 0x221E, "Infinity"},
  
  { 0x2227, "And"},
  { 0x2228, "Or"},
  { 0x2229, "Intersection"},
  { 0x222A, "Union"},
  { 0x222B, "Integral"},
  { 0x222E, "ContourIntegral"},
  { 0x222F, "DoubleContourIntegral"},
  { 0x2232, "ClockwiseContourIntegral"},
  { 0x2233, "CounterClockwiseContourIntegral"},
  
  { 0x2236, "Colon"},
  { 0x223C, "Tilde"},
  { 0x2241, "NotTilde"},
  { 0x2242, "EqualTilde"},
  { 0x2243, "TildeEqual"},
  { 0x2244, "NotTildeEqual"},
  { 0x2245, "TildeFullEqual"},
  { 0x2247, "NotTildeFullEqual"},
  { 0x2248, "TildeTilde"},
  { 0x2249, "NotTildeTilde"},
  
  { 0x224D, "CupCap"},
  { 0x224E, "HumpDownHump"},
  { 0x224F, "HumpEqual"},
  { 0x2250, "DotEqual"},
  
  { 0x2260, "NotEqual"},
  { 0x2261, "Congruent"},
  { 0x2262, "NotCongruent"},
  
  { 0x2264, "LessEqual"},
  { 0x2265, "GreaterEqual"},
  { 0x2266, "LessFullEqual"},
  { 0x2267, "GreaterFullEqual"},
  
  { 0x226A, "LessLess"},
  { 0x226B, "GreaterGreater"},
  
  { 0x226D, "NotCupCap"},
  { 0x226E, "NotLess"},
  { 0x226F, "NotGreater"},
  { 0x2270, "NotLessEqual"},
  { 0x2271, "NotGreaterEqual"},
  { 0x2272, "LessTilde"},
  { 0x2273, "GreaterTilde"},
  { 0x2274, "NotLessTilde"},
  { 0x2275, "NotGreaterTilde"},
  { 0x2276, "LessGreater"},
  { 0x2277, "GreaterLess"},
  { 0x2278, "NotLessGreater"},
  { 0x2279, "NotGreaterLess"},
  { 0x227A, "Precedes"},
  { 0x227B, "Succeeds"},
  { 0x227C, "PrecedesEqual"},
  { 0x227D, "SucceedsEqual"},
  { 0x227E, "PrecedesTilde"},
  { 0x227F, "SucceedsTilde"},
  { 0x2280, "NotPrecedes"},
  { 0x2281, "NotSucceeds"},
  { 0x2282, "Subset"},
  { 0x2283, "Superset"},
  { 0x2284, "NotSubset"},
  { 0x2285, "NotSuperset"},
  { 0x2286, "SubsetEqual"},
  { 0x2287, "SupersetEqual"},
  { 0x2288, "NotSubsetEqual"},
  { 0x2289, "NotSupersetEqual"},
  
  { 0x2295, "CirclePlus"},
  { 0x2296, "CircleMinus"},
  { 0x2297, "CircleTimes"},
  { 0x2299, "CircleDot"},
  
  { 0x22A2, "RightTee"},
  { 0x22A3, "LeftTee"},
  { 0x22A4, "DownTee"},
  { 0x22A5, "UpTee"},
  
  { 0x22B2, "LeftTriangle"},
  { 0x22B3, "RightTriangle"},
  { 0x22B4, "LeftTriangleEqual"},
  { 0x22B5, "RightTriangleEqual"},
  
  { 0x22BB, "Xor"},
  { 0x22BC, "Nand"},
  { 0x22BD, "Nor"},
  
  { 0x22C4, "Diamond"},
  
  { 0x22C6, "Star"},
  
  { 0x22DA, "LessEqualGreater"},
  { 0x22DB, "GreaterEqualLess"},
  
  { 0x22E0, "NotPrecedesEqual"},
  { 0x22E1, "NotSucceedsEqual"},
  
  { 0x22EA, "NotLeftTriangle"},
  { 0x22EB, "NotRightTriangle"},
  { 0x22EC, "NotLeftTriangleEqual"},
  { 0x22ED, "NotRightTriangleEqual"},
  { 0x22EE, "VerticalEllipsis"},
  { 0x22EF, "CenterEllipsis"},
  { 0x22F0, "AscendingEllipsis"},
  { 0x22F1, "DescendingEllipsis"},
  
  { 0x2308, "LeftCeiling"},
  { 0x2309, "RightCeiling"},
  { 0x230A, "LeftFloor"},
  { 0x230B, "RightFloor"},
  
  { 0x2329, "LeftAngleBracket"},
  { 0x232A, "RightAngleBracket"},
  
  { 0x27E6, "LeftDoubleBracket"},
  { 0x27E7, "RightDoubleBracket"},
  
  { 0x23B4, "OverBracket"},
  { 0x23B5, "UnderBracket"},
  
  { 0x23DC, "OverParenthesis"},
  { 0x23DD, "UnderParenthesis"},
  { 0x23DE, "OverBrace"},
  { 0x23DF, "UnderBrace"},
  
  { 0x001A, "SelectionPlaceholder"},
  { 0xFFFD, "Placeholder"},
  { 0x2192, "Rule"},
  { 0x21A6, "Function"},
  { 0x2254, "Assign"},
  { 0x29F4, "RuleDelayed"},
  { 0x2A74, "AssignDelayed"},
  
  // Private Use Area:
  { 0xF3BA, "SpanFromLeft"},
  { 0xF3BB, "SpanFromAbove"},
  { 0xF3BC, "SpanFromBoth"},
  { 0xF361, "Piecewise"},
  { 0xF362, "LeftInvisibleBracket"},
  { 0xF363, "RightInvisibleBracket"},
  { 0xF603, "LeftBracketingBar"},
  { 0xF604, "RightBracketingBar"},
  { 0xF605, "LeftDoubleBracketingBar"},
  { 0xF606, "RightDoubleBracketingBar"},
  { 0xF764, "AliasDelimiter"},
  { 0xF768, "AliasIndicator"}
};

//{ hashtable functions ...

static void destroy_nc(void *entry) {
  /* do nothing */
}

static unsigned int hash_nc_char(void *_entry) {
  struct named_char_t *entry = (struct named_char_t *)_entry;
  
  return entry->unichar;
}

static unsigned int hash_nc_name(void *_entry) {
  struct named_char_t *entry = (struct named_char_t *)_entry;
  
  return incremental_hash(entry->name, strlen(entry->name), 0);
}

static pmath_bool_t nc_nc_equal_chars(void *_entry1, void *_entry2) {
  struct named_char_t *entry1 = (struct named_char_t *)_entry1;
  struct named_char_t *entry2 = (struct named_char_t *)_entry2;
  
  return entry1->unichar == entry2->unichar;
}

static pmath_bool_t nc_nc_equal_names(void *_entry1, void *_entry2) {
  struct named_char_t *entry1 = (struct named_char_t *)_entry1;
  struct named_char_t *entry2 = (struct named_char_t *)_entry2;
  
  return strcmp(entry1->name, entry2->name) == 0;
}

static unsigned int nc_char_hash(void *key) {
  return *(uint32_t *)key;
}

static unsigned int nc_name_hash(void *_key) {
  const char *key = (const char *)_key;
  return incremental_hash(key, strlen(key), 0);
}

static pmath_bool_t nc_char_key_equal(void *_entry, void *key) {
  struct named_char_t *entry = (struct named_char_t *)_entry;
  
  return entry->unichar == *(uint32_t *)key;
}

static pmath_bool_t nc_name_key_equal(void *_entry, void *key) {
  struct named_char_t *entry = (struct named_char_t *)_entry;
  
  return strcmp(entry->name, (const char *)key) == 0;
}

//} ... hashtable functions

static pmath_hashtable_t name2char_ht;
static pmath_hashtable_t char2name_ht;
static const pmath_ht_class_t name2char_ht_class = { // key is a (char*)
  destroy_nc,
  hash_nc_name,
  nc_nc_equal_names,
  nc_name_hash,
  nc_name_key_equal
};
static const pmath_ht_class_t char2name_ht_class = { // key is a (uint32_t*)
  destroy_nc,
  hash_nc_char,
  nc_nc_equal_chars,
  nc_char_hash,
  nc_char_key_equal
};

PMATH_API
uint32_t pmath_char_from_name(const char *name) {
  struct named_char_t *entry = pmath_ht_search(name2char_ht, (void *)name);
  
  if(entry)
    return entry->unichar;
    
  return 0xFFFFFFFFU;
}

PMATH_API
const char *pmath_char_to_name(uint32_t unichar) {
  struct named_char_t *entry = pmath_ht_search(char2name_ht, &unichar);
  
  if(entry)
    return entry->name;
    
  return NULL;
}

static int hex(uint16_t ch) {
  if(ch >= '0' && ch <= '9')
    return ch - '0';
  if(ch >= 'a' && ch <= 'f')
    return ch - 'a' + 10;
  if(ch >= 'A' && ch <= 'F')
    return ch - 'A' + 10;
  return -1;
}

static uint16_t next_char(const uint16_t **str, const uint16_t *end, pmath_bool_t *err) {
  size_t maxlen = end - *str;
  uint16_t ch;
  
  if(maxlen == 0) {
    *err = TRUE;
    return 0;
  }
  
  ch = (*str)[0];
  if(ch == '\\') {
    if(1 < maxlen && (*str)[1] == '\n') {
      size_t i = 1;
      while(i < maxlen && (*str)[i] <= ' ')
        ++i;
        
      if(i < maxlen) {
        ch = (*str)[i];
        *str += i + 1;
        return ch;
      }
    }
    
    ++*str;
    *err = TRUE;
    return 0;
  }
  
  ++*str;
  return ch;
}

static const uint16_t *skip_all_hex(const uint16_t *str, int maxlen) {
  const uint16_t *end = str + maxlen;
  pmath_bool_t err = FALSE;
  
  while(str != end && !err) {
    const uint16_t *next_str = str;
    uint16_t ch = next_char(&next_str, end, &err);
    
    if(!pmath_char_is_hexdigit(ch))
      break;
      
    str = next_str;
  }
  
  return str;
}

PMATH_API const uint16_t *pmath_char_parse(const uint16_t *str, int maxlen, uint32_t *result) {
  assert(str    != NULL);
  assert(result != NULL);
  
  *result = 0xFFFFFFFFU;
  if(maxlen <= 0)
    return str;
    
  if(str[0] != '\\') {
    if((str[0] & 0xFC00) == 0xD800) {
      if(maxlen == 1 || (str[1] & 0xFC00) != 0xDC00)
        return str + 1;
        
      *result = 0x10000 | (((uint32_t)str[0] & 0x03FF) << 10) | (str[1] & 0x03FF);
      return str + 2;
    }
    
    *result = str[0];
    return str + 1;
  }
  
  if(maxlen < 2)
    return str + 1;
    
  switch(str[1]) {
    case '\\':
    case '"':
      *result = str[1];
      return str + 2;
      
    case 'n':
      *result = '\n';
      return str + 2;
      
    case 'r':
      *result = '\r';
      return str + 2;
      
    case 't':
      *result = '\t';
      return str + 2;
      
    case '(':
      *result = PMATH_CHAR_LEFT_BOX;
      return str + 2;
      
    case ')':
      *result = PMATH_CHAR_RIGHT_BOX;
      return str + 2;
      
    case 'x': {
        pmath_bool_t err = FALSE;
        const uint16_t *end = str + maxlen;
        const uint16_t *s = str + 2;
        
        int h1 = hex(next_char(&s, end, &err));
        int h2 = hex(next_char(&s, end, &err));
        if(!err && h1 >= 0 && h2 >= 0) {
          *result = (uint32_t)((h1 << 4) | h2);
          return s;
        }
      }
      return skip_all_hex(str + 2, maxlen - 2);
      
    case 'u': {
        pmath_bool_t err = FALSE;
        const uint16_t *end = str + maxlen;
        const uint16_t *s = str + 2;
        
        int h1 = hex(next_char(&s, end, &err));
        int h2 = hex(next_char(&s, end, &err));
        int h3 = hex(next_char(&s, end, &err));
        int h4 = hex(next_char(&s, end, &err));
        
        if(!err && h1 >= 0 && h2 >= 0 && h3 >= 0 && h4 >= 0) {
          *result = (uint32_t)((h1 << 12) | (h2 << 8) | (h3 << 4) | h4);
          return str + 6;
        }
      }
      return skip_all_hex(str + 2, maxlen - 2);
      
    case 'U': {
        pmath_bool_t err = FALSE;
        const uint16_t *end = str + maxlen;
        const uint16_t *s = str + 2;
        
        int h1 = hex(next_char(&s, end, &err));
        int h2 = hex(next_char(&s, end, &err));
        int h3 = hex(next_char(&s, end, &err));
        int h4 = hex(next_char(&s, end, &err));
        int h5 = hex(next_char(&s, end, &err));
        int h6 = hex(next_char(&s, end, &err));
        int h7 = hex(next_char(&s, end, &err));
        int h8 = hex(next_char(&s, end, &err));
        
        if( !err &&
            h1 >= 0 && h2 >= 0 && h3 >= 0 && h4 >= 0 &&
            h5 >= 0 && h6 >= 0 && h7 >= 0 && h8 >= 0)
        {
          uint32_t u = ((uint32_t)h1) << 28;
          u |= h2 << 24;
          u |= h3 << 20;
          u |= h4 << 16;
          u |= h5 << 12;
          u |= h6 <<  8;
          u |= h7 <<  4;
          u |= h8;
          
          if(u <= 0x10FFFF) {
            *result = u;
            return str + 10;
          }
        }
      }
      return skip_all_hex(str + 2, maxlen - 2);
      
    case '[': {
        char s[64];
        
        int e = 2;
        int n = 0;
        
        while(e < maxlen &&
              n < (int)sizeof(s) - 1 &&
              str[e] <= 0x7F &&
              str[e] != ']' &&
              str[e] != '[')
        {
          if(str[e] == '\\') {
            if(e + 1 < maxlen && str[e + 1] == '\n') {
              ++e;
              while(e < maxlen && str[e] <= ' ')
                ++e;
                
              continue;
            }
            else
              break;
          }
          
          s[n++] = (char)str[e++];
        }
        
        if(e < maxlen && str[e] == ']' && n < (int)sizeof(s) - 1) {
          s[n] = '\0';
          *result = pmath_char_from_name(s);
          if(*result != 0xFFFFFFFFU)
            return str + e + 1;
        }
        
        return str + e;
        //return str + 2;
      }
  }
  
  return str + 1;
}

PMATH_PRIVATE pmath_bool_t _pmath_charnames_init(void) {
  size_t i;
  
  name2char_ht = pmath_ht_create(&name2char_ht_class, NAMED_CHAR_ARR_COUNT * 2 / 3);
  char2name_ht = pmath_ht_create(&char2name_ht_class, NAMED_CHAR_ARR_COUNT * 2 / 3);
  if(!name2char_ht || !char2name_ht) {
    pmath_ht_destroy(name2char_ht);
    pmath_ht_destroy(char2name_ht);
    return FALSE;
  }
  
  for(i = 0; i < NAMED_CHAR_ARR_COUNT; ++i) {
    void *dummy;
    dummy = pmath_ht_insert(name2char_ht, &named_char_array[i]);
    if(dummy && dummy != &named_char_array[i]) {
      pmath_debug_print("name used for multipe chars: \[%s] for U+%04X and U+%04X\n",
                        named_char_array[i].name,
                        (unsigned int)named_char_array[i].unichar,
                        ((struct named_char_t *)dummy)->unichar);
    }
    
    dummy = pmath_ht_insert(char2name_ht, &named_char_array[i]);
    
    if(dummy && dummy != &named_char_array[i]) {
      pmath_debug_print("duplicate character: U+%04X = \\[%s] = \\[%s]\n",
                        (unsigned int)named_char_array[i].unichar,
                        named_char_array[i].name,
                        ((struct named_char_t *)dummy)->name);
    }
  }
  
  return TRUE;
}

PMATH_PRIVATE void _pmath_charnames_done(void) {
  pmath_ht_destroy(name2char_ht);
  pmath_ht_destroy(char2name_ht);
}
