#include <util/text-gathering.h>

#include <boxes/abstractsequence.h>
#include <boxes/ownerbox.h>
#include <boxes/paneselectorbox.h>
#include <boxes/sectionlist.h>


using namespace richmath;

namespace richmath {
  class SimpleTextGather::Impl {
    public:
      Impl(SimpleTextGather &self);
      
      void append_pane(PaneSelectorBox *pane);
      void append_section(AbstractSequenceSection *sect);
      void append_section_list(SectionList *sects);
      void append_sequence(AbstractSequence *seq);
  
    private:
      SimpleTextGather &self;
  };
}

//{ class SimpleTextGather ...

SimpleTextGather::SimpleTextGather(int maxlen)
: max_length(maxlen)
{
}

void SimpleTextGather::append(Box *box) {
  if(text.length() >= max_length)
    return;
  
  if(auto seq = dynamic_cast<AbstractSequence*>(box)) {
    Impl(*this).append_sequence(seq);
    return;
  }
  
  if(auto owner = dynamic_cast<OwnerBox*>(box)) {
    Impl(*this).append_sequence(owner->content());
    return;
  }
  
  if(auto sect = dynamic_cast<AbstractSequenceSection*>(box)) {
    Impl(*this).append_section(sect);
    return;
  }
  
  if(auto pane = dynamic_cast<PaneSelectorBox*>(box)) {
    Impl(*this).append_pane(pane);
    return;
  }
}

void SimpleTextGather::append_space() {
  int oldlen = text.length();
  if(oldlen >= max_length || oldlen == 0)
    return;
  
  if(text.buffer()[oldlen - 1] != ' ')
    text.insert(oldlen, " ", 1);
}

void SimpleTextGather::append_text(String str) {
  int inslen = str.length();
  if(!inslen)
    return;
  
  int oldlen = text.length();
  if(oldlen >= max_length)
    return;
  
  if(oldlen + inslen <= max_length)
    text += str;
  else
    text += str.part(0, max_length - oldlen);
}

void SimpleTextGather::append_text(const char *str) {
  int inslen = strlen(str);
  if(!inslen)
    return;
  
  int oldlen = text.length();
  if(oldlen >= max_length)
    return;
  
  if(oldlen + inslen <= max_length)
    text.insert(oldlen, str, inslen);
  else
    text.insert(oldlen, str, max_length - oldlen);
}

//} ... class SimpleTextGather

//{ class SimpleTextGather::Impl ...

inline SimpleTextGather::Impl::Impl(SimpleTextGather &self)
: self{self} 
{
}


void SimpleTextGather::Impl::append_pane(PaneSelectorBox *pane) {
  int i = pane->current_selection();
  if(0 <= i && i < pane->count())
    self.append(pane->item(i));
}

void SimpleTextGather::Impl::append_section(AbstractSequenceSection *sect) {
  if(String lbl = sect->get_style(SectionLabel)) {
    self.append_text(PMATH_CPP_MOVE(lbl));
    self.append_space();
  }
  
  if(Box *dingbat = sect->dingbat().box_or_null()) {
    self.append(dingbat);
    self.append_space();
  }
  
  append_sequence(sect->content());
}

void SimpleTextGather::Impl::append_section_list(SectionList *sects) {
  int count = sects->count();
  
  if(count == 0)
    return;
  
  self.append(sects->section(0));
  for(int i = 1; i < count; ++i) {
    self.append_text("\n\n");
    self.append(sects->section(i));
  }
}

void SimpleTextGather::Impl::append_sequence(AbstractSequence *seq) {
  seq->ensure_boxes_valid();
  
  int start = 0;
  for(int i = 0; i < seq->count(); ++i) {
    int oldlen = self.text.length();
    if(oldlen >= self.max_length)
      return;
    
    Box *item    = seq->item(i);
    int  next    = item->index();
    int  sub_len = next - start;
    if(sub_len >= self.max_length - oldlen) {
      self.text += seq->text().part(start, self.max_length - oldlen);
      return;
    }
    
    self.text += seq->text().part(start, sub_len);
    self.append(item);
    
    start = next + 1;
  }
  
  if(int rest_len = seq->length() - start) {
    int oldlen = self.text.length();
    if(oldlen + rest_len <= self.max_length)
      self.text += seq->text().part(start, rest_len);
    else
      self.text += seq->text().part(start, self.max_length - oldlen);
  }
}

//} ... class SimpleTextGather::Impl
