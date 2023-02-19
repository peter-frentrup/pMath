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
      
      void append_box(Box *box);
    
    private:
      void append_box_dispatch(Box *box);
      void append_pane(PaneSelectorBox *pane);
      void append_section(AbstractSequenceSection *sect);
      void append_section_list(SectionList *sects);
      void append_sequence(AbstractSequence *seq);
      
      void append_text_for(AbstractSequence *seq, String str);
    
    private:
      SimpleTextGather &self;
  };
}

//{ class SimpleTextGather ...

SimpleTextGather::SimpleTextGather(int maxlen)
: max_length(maxlen),
  skip_string_characters_in_math(false)
{
}

void SimpleTextGather::append(Box *box) {
  if(!box)
    return;
  
  bool old_skip_string_characters = skip_string_characters_in_math;
  
  skip_string_characters_in_math = !box->get_style(ShowStringCharacters, !skip_string_characters_in_math);
  
  Impl(*this).append_box(box);
  
  skip_string_characters_in_math = old_skip_string_characters;
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

void SimpleTextGather::Impl::append_box(Box *box) {
  if(self.text.length() >= self.max_length)
    return;
  
  if(!box)
    return;
  
  bool box_show_string_characters = box->get_own_style(ShowStringCharacters, !self.skip_string_characters_in_math);
  if(!box_show_string_characters != self.skip_string_characters_in_math) {
    bool old = self.skip_string_characters_in_math;
    self.skip_string_characters_in_math = !box_show_string_characters;
    
    append_box_dispatch(box);
    
    self.skip_string_characters_in_math = old;
  }
  else {
    append_box_dispatch(box);
  }
}

void SimpleTextGather::Impl::append_box_dispatch(Box *box) {
  if(auto seq = dynamic_cast<AbstractSequence*>(box)) {
    append_sequence(seq);
    return;
  }
  
  if(auto owner = dynamic_cast<OwnerBox*>(box)) {
    append_sequence(owner->content());
    return;
  }
  
  if(auto sect = dynamic_cast<AbstractSequenceSection*>(box)) {
    append_section(sect);
    return;
  }
  
  if(auto pane = dynamic_cast<PaneSelectorBox*>(box)) {
    append_pane(pane);
    return;
  }
}

void SimpleTextGather::Impl::append_pane(PaneSelectorBox *pane) {
  int i = pane->current_selection();
  if(0 <= i && i < pane->count())
    append_box(pane->item(i));
}

void SimpleTextGather::Impl::append_section(AbstractSequenceSection *sect) {
  if(String lbl = sect->get_style(SectionLabel)) {
    self.append_text(PMATH_CPP_MOVE(lbl));
    self.append_space();
  }
  
  if(Box *dingbat = sect->dingbat().box_or_null()) {
    append_box(dingbat);
    self.append_space();
  }
  
  append_sequence(sect->content());
}

void SimpleTextGather::Impl::append_section_list(SectionList *sects) {
  int count = sects->count();
  
  if(count == 0)
    return;
  
  append_box(sects->section(0));
  for(int i = 1; i < count; ++i) {
    self.append_text("\n\n");
    append_box(sects->section(i));
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
      append_text_for(seq, seq->text().part(start, self.max_length - oldlen));
      return;
    }
    
    append_text_for(seq, seq->text().part(start, sub_len));
    append_box(item);
    
    start = next + 1;
  }
  
  if(int rest_len = seq->length() - start) {
    int oldlen = self.text.length();
    if(oldlen + rest_len <= self.max_length)
      append_text_for(seq, seq->text().part(start, rest_len));
    else
      append_text_for(seq, seq->text().part(start, self.max_length - oldlen));
  }
}

void SimpleTextGather::Impl::append_text_for(AbstractSequence *seq, String str) {
  if(self.skip_string_characters_in_math && seq->kind() == LayoutKind::Math) {
    const uint16_t *buf = str.buffer();
    int             len = str.length();
    
    while(len > 0) {
      int n = 0;
      while(n < len && buf[n] != '"' && buf[n] != '\\')
        ++n;
      
      if(n > 0) {
        self.text.insert(INT_MAX, buf, n);
        buf+= n;
        len-= n;
      }
      
      
      if(len < 2)
        return;
      
      if(buf[0] == '\\') {
        self.text += buf[1];
        buf+= 2;
        len-= 2;
      }
      else {
        ++buf;
        --len;
      }
    }
    
    return;
  }
  
  self.text += str;
}

//} ... class SimpleTextGather::Impl
