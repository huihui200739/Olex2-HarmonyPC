#include "generation_options.h"
#include "estrlist.h"
#include "exception.h"
namespace olex2::harmony::ui {
double ParseOriginalGenerationValue(const olxstr& text,double previous){
  try{return text.ToDouble();}
  catch(const TInvalidFloatNumberException&){return previous;}
}
GenerationValues ParseOriginalGenerationResult(const GenerationDraft& draft){
  GenerationValues result;double value=2;
  for(size_t i=0;i<3;++i){value=ParseOriginalGenerationValue(olxstr(draft.from[i].c_str()),value);result.from[i]=static_cast<float>(value);}
  for(size_t i=0;i<3;++i){value=ParseOriginalGenerationValue(olxstr(draft.to[i].c_str()),value);result.to[i]=static_cast<float>(value);}
  return result;
}
}
