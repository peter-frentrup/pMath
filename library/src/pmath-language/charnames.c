#include <pmath-language/charnames.h>

#include <pmath-util/debug.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/incremental-hash-private.h>

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
  {0x1D4BA, "ScriptE"},
  {0x1D4BB, "ScriptF"},
  {0x1D4BC, "ScriptG"},
  {0x1D4BD, "ScriptH"},
  {0x1D4BE, "ScriptI"},
  {0x1D4BF, "ScriptJ"},
  {0x1D4C0, "ScriptK"},
  {0x1D4C1, "ScriptL"},
  {0x1D4C2, "ScriptM"},
  {0x1D4C3, "ScriptN"},
  {0x1D4C4, "ScriptO"},
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
  {0x1D51A, "GothicCapitaLW"},
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
  
  { 0x00AC, "Not"},
  { 0x00B0, "Degree"},
  { 0x00B1, "PlusMinus"},
  //{ 0x00B7, "CenterDot"},
  { 0x00D7, "Times"},
  { 0x00F7, "Divide"},
  { 0x22C5, "Dot"},
  { 0x2A2F, "Cross"},
  
  { 0x2020, "Dagger"},
  { 0x2021, "DoubleDagger"},
  { 0x2026, "Ellipsis"},
  { 0x22EE, "VerticalEllipsis"},
  { 0x22EF, "CenterEllipsis"},
  { 0x22F0, "AscendingEllipsis"},
  { 0x22F1, "DescendingEllipsis"},
  
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
  { 0x22BB, "Xor"},
  { 0x22BC, "Nand"},
  { 0x22BD, "Nor"},
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
  { 0x2260, "NotEqual"},
  { 0x2261, "Congruent"},
  { 0x2262, "NotCongruent"},
  { 0x2264, "LessEqual"},
  { 0x2265, "GreaterEqual"},
  { 0x226E, "NotLess"},
  { 0x226F, "NotGreater"},
  { 0x2270, "NotLessEqual"},
  { 0x2271, "NotGreaterEqual"},
  { 0x2272, "LessTilde"},
  { 0x2273, "GreaterTilde"},
  { 0x2274, "NotLessTilde"},
  { 0x2275, "NotGreaterTilde"},
  
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
  { 0x22C4, "Diamond"},
  { 0x22C6, "Star"},
  
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
  { 0x2192, "Rule"},
  { 0x21A6, "Function"},
  { 0x2254, "Assign"},
  { 0x29F4, "RuleDelayed"},
  { 0x2A74, "AssignDelayed"},
  { 0xF3BA, "SpanFromLeft"},
  { 0xF3BB, "SpanFromAbove"},
  { 0xF3BC, "SpanFromBoth"},
  { 0xF361, "Piecewise"},
  { 0xF764, "AliasDelimiter"},
  { 0xF768, "AliasIndicator"},
  { 0xFFFD, "Placeholder"}
};

//{ hashtable functions ...

static void destroy_nc(void *entry){
  /* do nothing */
}

static unsigned int hash_nc_char(void *_entry){
  struct named_char_t *entry = (struct named_char_t*)_entry;
  
  return entry->unichar;
}

static unsigned int hash_nc_name(void *_entry){
  struct named_char_t *entry = (struct named_char_t*)_entry;
  
  return incremental_hash(entry->name, strlen(entry->name), 0);
}

static pmath_bool_t nc_nc_equal_chars(void *_entry1, void *_entry2){
  struct named_char_t *entry1 = (struct named_char_t*)_entry1;
  struct named_char_t *entry2 = (struct named_char_t*)_entry2;
  
  return entry1->unichar == entry2->unichar;
}

static pmath_bool_t nc_nc_equal_names(void *_entry1, void *_entry2){
  struct named_char_t *entry1 = (struct named_char_t*)_entry1;
  struct named_char_t *entry2 = (struct named_char_t*)_entry2;
  
  return strcmp(entry1->name, entry2->name) == 0;
}

static unsigned int nc_char_hash(void *key){
  return *(uint32_t*)key;
}

static unsigned int nc_name_hash(void *_key){
  const char *key = (const char*)_key;
  return incremental_hash(key, strlen(key), 0);
}

static pmath_bool_t nc_char_key_equal(void *_entry, void *key){
  struct named_char_t *entry = (struct named_char_t*)_entry;
  
  return entry->unichar == *(uint32_t*)key;
}

static pmath_bool_t nc_name_key_equal(void *_entry, void *key){
  struct named_char_t *entry = (struct named_char_t*)_entry;
  
  return strcmp(entry->name, (const char*)key) == 0;
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
uint32_t pmath_char_from_name(const char *name){
  struct named_char_t *entry = pmath_ht_search(name2char_ht, (void*)name);
  
  if(entry)
    return entry->unichar;
  
  return 0xFFFFFFFFU;
}

PMATH_API
const char *pmath_char_to_name(uint32_t unichar){
  struct named_char_t *entry = pmath_ht_search(char2name_ht, &unichar);
  
  if(entry)
    return entry->name;
  
  return NULL;
}

PMATH_PRIVATE pmath_bool_t _pmath_charnames_init(void){
  size_t i;
  
  name2char_ht = pmath_ht_create(&name2char_ht_class, NAMED_CHAR_ARR_COUNT * 2 / 3);
  char2name_ht = pmath_ht_create(&char2name_ht_class, NAMED_CHAR_ARR_COUNT * 2 / 3);
  if(!name2char_ht || !char2name_ht){
    pmath_ht_destroy(name2char_ht);
    pmath_ht_destroy(char2name_ht);
    return FALSE;
  }
  
  for(i = 0;i < NAMED_CHAR_ARR_COUNT;++i){
    void *dummy;
    dummy = pmath_ht_insert(name2char_ht, &named_char_array[i]);
    if(dummy && dummy != &named_char_array[i]){
      pmath_debug_print("name used for multipe chars: \[%s] for U+%04X and U+%04X\n",
        named_char_array[i].name,
        (unsigned int)named_char_array[i].unichar,
        ((struct named_char_t*)dummy)->unichar);
    }
    
    dummy = pmath_ht_insert(char2name_ht, &named_char_array[i]);
    
    if(dummy && dummy != &named_char_array[i]){
      pmath_debug_print("duplicate character: U+%04X = \\[%s] = \\[%s]\n", 
        (unsigned int)named_char_array[i].unichar,
        named_char_array[i].name,
        ((struct named_char_t*)dummy)->name);
    }
  }
  
  return TRUE;
}

PMATH_PRIVATE void _pmath_charnames_done(void){
  pmath_ht_destroy(name2char_ht);
  pmath_ht_destroy(char2name_ht);
}
