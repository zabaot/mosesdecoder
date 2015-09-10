#include <vector>
#include "MorphoLM.h"
#include "moses/ScoreComponentCollection.h"
#include "moses/Hypothesis.h"
#include "moses/FactorCollection.h"
#include "util/exception.hh"

#include "moses/FF/JoinScore/TrieSearch.h"
#include "moses/FF/MorphoTrie/MorphTrie.h"

using namespace std;

namespace Moses
{
int MorphoLMState::Compare(const FFState& other) const
{
  const MorphoLMState &otherState = static_cast<const MorphoLMState&>(other);

  if (m_lastWords < otherState.m_lastWords) {
	  return -1;
  }
  else if (m_lastWords > otherState.m_lastWords) {
	  return +1;
  }

  // context words equal. Compare last unfinished word
  if (m_unfinishedWord < otherState.m_unfinishedWord) {
	  return -1;
  }
  else if (m_unfinishedWord > otherState.m_unfinishedWord) {
	  return +1;
  }

  return 0;
}

////////////////////////////////////////////////////////////////
MorphoLM::MorphoLM(const std::string &line)
:StatefulFeatureFunction(1, line)
,m_order(0)
,m_factorType(0)
,m_marker("+")
{
  ReadParameters();

  if (m_order == 0) {
	UTIL_THROW2("Must set order");
  }

  FactorCollection &fc = FactorCollection::Instance();
  m_sentenceStart = fc.AddFactor("<s>", false);
  m_sentenceEnd = fc.AddFactor("</s>", false);
}

const FFState* MorphoLM::EmptyHypothesisState(const InputType &input) const {
  std::vector<const Factor*> context;
  context.push_back(m_sentenceStart);

  return new MorphoLMState(context);
}

void MorphoLM::Load()
{
  root = new MorphTrie<string, float>;
  LoadLm(m_path, root);
}

void MorphoLM::EvaluateInIsolation(const Phrase &source
    , const TargetPhrase &targetPhrase
    , ScoreComponentCollection &scoreBreakdown
    , ScoreComponentCollection &estimatedFutureScore) const
{}

void MorphoLM::EvaluateWithSourceContext(const InputType &input
    , const InputPath &inputPath
    , const TargetPhrase &targetPhrase
    , const StackVec *stackVec
    , ScoreComponentCollection &scoreBreakdown
    , ScoreComponentCollection *estimatedFutureScore) const
{}

void MorphoLM::EvaluateTranslationOptionListWithSourceContext(const InputType &input
    , const TranslationOptionList &translationOptionList) const
{}

FFState* MorphoLM::EvaluateWhenApplied(
  const Hypothesis& cur_hypo,
  const FFState* prev_state,
  ScoreComponentCollection* accumulator) const
{
  // dense scores
  float score = 0;
  bool prevIsMorph = false;
  string prevMorph = "";
  size_t targetLen = cur_hypo.GetCurrTargetPhrase().GetSize();
  const WordsRange &targetRange = cur_hypo.GetCurrTargetWordsRange();

  assert(prev_state);

  const MorphoLMState *prevMorphState = static_cast<const MorphoLMState*>(prev_state);
  std::vector<const Factor*> context(prevMorphState->GetPhrase());
          
  //FactorCollection &fc = FactorCollection::Instance();

  for (size_t pos = targetRange.GetStartPos(); pos < targetLen; ++pos){
	  const Word &word = cur_hypo.GetWord(pos);
	  const Factor *factor = word[m_factorType];
	  string str = factor->GetString().as_string();
          if (str[0] == '+' && prevIsMorph == true) {
            //TODO combine morphemes
            //str = prevMorph.pop_back() + str.erase(str.begin()); //Combine two morphemes and delete +s
            //TODO score
          }
          else if (str[0] == '+' && prevIsMorph == false) {
            //TODO Give bad score
            str.erase(str.begin()); //Get rid of starting +
          }
          else if (str[0] != '+' && prevIsMorph == true) {
            //TODO GIve bad score
             //prevMorph.pop_back(); // Get rid of that trailing +
          }
          else {
            //Yay! Easy ... just words
          }
          if (str.at(str.length() -1) == '+') {
        	  prevIsMorph = true;
                  prevMorph = str;
            //TODO estimate score of the rest
          }
          else {
        	  prevIsMorph = false;
                  prevMorph = "";
             // TODO: Subtract itermediate?
          }
          //m_sentenceStart = fc.AddFactor(str, false);
	  //context.push_back(factor);
	  // score TODO
          //score += MorphoLM::KneserNey(factor); // factor is int& ... fix this

  }

  // finished scoring. set score
  accumulator->PlusEquals(this, score);

  // TODO: Subtract itermediate?

  return new MorphoLMState(context);
}

FFState* MorphoLM::EvaluateWhenApplied(
  const ChartHypothesis& /* cur_hypo */,
  int /* featureID - used to index the state in the previous hypotheses */,
  ScoreComponentCollection* accumulator) const
{
  abort();
  return NULL;
}

float MorphoLM::KneserNey(std::vector<int>* context)
{
  float oov = -10000000000.0;
  float delta = 1.0;
  float prob = 0.0;
  float backoff = 0.0;

  //p = max() / count[order] + delta * N1 * p_1(backoff)/count[order];

  float prob = root.getProb(context);

  if (prob != 0.0) 
    return prob;
  
  if (context.size() > 0) {
    backoff = root.getBackoff(context);
    context.pop_front();
    return (backoff + MorphoLM::KneserNey(context));
  }
  
  return oov;
}

void MorphoLM::SetParameter(const std::string& key, const std::string& value)
{
  if (key == "path") {
    m_path = value;
  }
  else if (key == "order") {
    m_order = Scan<size_t>(value);
  }
  else if (key == "factor") {
	m_factorType = Scan<FactorType>(value);
  }
  else if (key == "marker") {
	  m_marker = value;
  }
  else {
    StatefulFeatureFunction::SetParameter(key, value);
  }
}

}

