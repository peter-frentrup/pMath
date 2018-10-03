#include <pmath-core/numbers.h>

#include <pmath-language/tokens.h>
#include <pmath-language/scanner.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>

extern pmath_symbol_t pmath_System_FractionBox;
extern pmath_symbol_t pmath_System_FrameBox;
extern pmath_symbol_t pmath_System_GridBox;
extern pmath_symbol_t pmath_System_InterpretationBox;
extern pmath_symbol_t pmath_System_OverscriptBox;
extern pmath_symbol_t pmath_System_RadicalBox;
extern pmath_symbol_t pmath_System_SqrtBox;
extern pmath_symbol_t pmath_System_StyleBox;
extern pmath_symbol_t pmath_System_SubsuperscriptBox;
extern pmath_symbol_t pmath_System_TagBox;
extern pmath_symbol_t pmath_System_UnderoverscriptBox;
extern pmath_symbol_t pmath_System_UnderscriptBox;

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_prepare_shallow(
  pmath_t obj,   // will be freed
  size_t         maxdepth,
  size_t         maxlength
) {
  if(pmath_is_expr(obj)) {
    size_t i, len = pmath_expr_length(obj);
    
    if(maxdepth <= 2) {
      pmath_t head = pmath_expr_get_item(obj, 0);
      pmath_unref(obj);
      
      return pmath_expr_new_extended(
               head, 1,
               pmath_expr_new_extended(
                 pmath_ref(PMATH_SYMBOL_SKELETON), 1,
                 pmath_integer_new_uiptr(len)));
    }
    
    if(len > maxlength) {
      pmath_t fst = pmath_expr_get_item_range(obj, 1, maxlength);
      pmath_unref(obj);
      
      for(i = 1; i <= maxlength; ++i) {
        fst = pmath_expr_set_item(
                fst, i,
                _pmath_prepare_shallow(
                  pmath_expr_get_item(fst, i),
                  maxdepth - 1,
                  maxlength));
      }
      
      return pmath_expr_append(
               fst, 1,
               pmath_expr_new_extended(
                 pmath_ref(PMATH_SYMBOL_SKELETON), 1,
                 pmath_integer_new_uiptr(len - maxlength)));
    }
    
    for(i = 1; i <= len; ++i) {
      obj = pmath_expr_set_item(
              obj, i,
              _pmath_prepare_shallow(
                pmath_expr_get_item(obj, i),
                maxdepth - 1,
                maxlength));
    }
    
    return obj;
  }
  
  return obj;
}

PMATH_PRIVATE
long _pmath_boxes_length(pmath_t boxes) { // boxes wont be freed
  if(pmath_is_string(boxes)) {
    const uint16_t *buf = pmath_string_buffer(&boxes);
    int len             = pmath_string_length(boxes);
    
    if(buf[0] == '"' && len > 0) {
      long result = 0;
      int i = 1;
      
      if(buf[len - 1] == '"')
        --len;
        
      while(i < len) {
        if(buf[i] == '\\')
          ++i;
          
        ++result;
        ++i;
      }
      
      return result;
    }
    
    return len + 1;
  }
  
  if(pmath_is_expr(boxes)) {
    long result = 0;
    size_t i, len = pmath_expr_length(boxes);
    pmath_t item = pmath_expr_get_item(boxes, 0);
    pmath_unref(item);
    
    if( pmath_same(item, PMATH_SYMBOL_RULE) ||
        pmath_same(item, PMATH_SYMBOL_RULEDELAYED))
    {
      return 0;
    }
    
    if( pmath_same(item, pmath_System_FractionBox)        ||
        pmath_same(item, pmath_System_OverscriptBox)      ||
        pmath_same(item, pmath_System_SubsuperscriptBox)  ||
        pmath_same(item, pmath_System_UnderoverscriptBox) ||
        pmath_same(item, pmath_System_UnderscriptBox))
    {
      long sub;
      
      for(i = 0; i <= len; ++i) {
        item = pmath_expr_get_item(boxes, i);
        
        sub = _pmath_boxes_length(item);
        if(result < sub)
          result = sub;
          
        pmath_unref(item);
      }
      
      return result;
    }
    
    if(pmath_same(item, pmath_System_GridBox)) {
      size_t rows, cols;
      pmath_t matrix = pmath_expr_get_item(boxes, 1);
      
      if(_pmath_is_matrix(matrix, &rows, &cols, FALSE)) {
        size_t j;
        
        for(i = 1; i <= cols; ++i) {
          pmath_t row;
          long colwidth = 0;
          long sub;
          
          for(j = 1; j <= rows; ++j) {
            row  = pmath_expr_get_item(matrix, j);
            item = pmath_expr_get_item(row, i);
            
            sub = _pmath_boxes_length(item);
            if(colwidth < sub)
              colwidth = sub;
              
            pmath_unref(item);
            pmath_unref(row);
          }
          
          result += colwidth + 1;
        }
        
        pmath_unref(matrix);
        return result;
      }
      
      pmath_unref(matrix);
    }
    
    for(i = 0; i <= len; ++i) {
      item = pmath_expr_get_item(boxes, i);
      
      result += _pmath_boxes_length(item);
      
      pmath_unref(item);
    }
    
    return result;
  }
  
  return 0;
}

static pmath_bool_t is_operand(pmath_t box) {
  if(pmath_is_string(box)) {
    const uint16_t *buf = pmath_string_buffer(&box);
    pmath_token_t tok;
    
    if(pmath_string_length(box) == 0)
      return FALSE;
      
    tok = pmath_token_analyse(buf, 1, NULL);
    return tok == PMATH_TOK_DIGIT ||
           tok == PMATH_TOK_STRING ||
           tok == PMATH_TOK_NAME;
  }
  
  return TRUE;
}

static pmath_bool_t is_operand_at(pmath_expr_t boxes, size_t i) {
  pmath_t box = pmath_expr_get_item(boxes, i);
  pmath_bool_t result = is_operand(box);
  pmath_unref(box);
  return result;
}

static long sub_length(pmath_expr_t boxes, size_t i) {
  pmath_t box = pmath_expr_get_item(boxes, i);
  long result = _pmath_boxes_length(box);
  pmath_unref(box);
  return result;
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_shorten_boxes(pmath_t boxes, long length) {
  if(pmath_is_string(boxes)) {
    const uint16_t *buf = pmath_string_buffer(&boxes);
    int len             = pmath_string_length(boxes);
    
    if(len > length && len > 2) {
      if( len > length + 10   &&
          buf[0]       == '"' &&
          buf[len - 1] == '"' &&
          buf[len - 2] != '\\')
      {
        pmath_string_t fst, snd;
        length = (length - 2) / 2;
        
        fst = pmath_string_part(pmath_ref(boxes), 0, length);
        snd = pmath_string_part(boxes, len - length, -1);
        
        fst = pmath_string_concat(fst, PMATH_C_STRING("\""));
        snd = pmath_string_concat(PMATH_C_STRING("\""), snd);
        
        return pmath_build_value("(ooo)",
                                 fst,
                                 pmath_evaluate(
                                   pmath_parse_string_args(
                                     "ToBoxes(Skeleton(`1`))", "(i)", len - 2 * length)),
                                 snd);
      }
      
      if( len > length + 10 &&
          buf[0] >= '0' &&
          buf[0] <= '9')
      {
        int i;
        for(i = 1; i < len; ++i) {
          if(buf[i] < '0' || buf[i] > '9')
            break;
        }
        
        if(i == len) { // integer
          pmath_string_t fst, snd;
          length = (length - 2) / 2;
          
          fst = pmath_string_part(pmath_ref(boxes), 0, length);
          snd = pmath_string_part(boxes, len - length, -1);
          
          return pmath_build_value(
                   "(ooo)",
                   fst,
                   pmath_evaluate(
                     pmath_parse_string_args(
                       "ToBoxes(Skeleton(`1`))", "(i)", len - 2 * length)),
                   snd);
        }
      }
      
      
      pmath_unref(boxes);
      return pmath_evaluate(
               pmath_parse_string_args(
                 "ToBoxes(Skeleton(`1`))", "(i)", len));
    }
    
    return boxes;
  }
  
  if(pmath_is_expr(boxes)) {
    size_t len = pmath_expr_length(boxes);
    pmath_t item = pmath_expr_get_item(boxes, 0);
    pmath_unref(item);
    
    if(pmath_same(item, PMATH_SYMBOL_LIST)) {
      size_t left  = 1;
      size_t right = len;
      size_t prevleft  = left;
      size_t prevright = right;
      long leftlength  = 0;
      long rightlength = 0;
      long sublength = 0;
      
      while(left <= right) {
        if(leftlength <= rightlength) {
          sublength = sub_length(boxes, left++);
          
          if(left <= right && !is_operand_at(boxes, left))
            sublength += sub_length(boxes, left++);
        }
        else {
          sublength = 0;
          if(is_operand_at(boxes, right))
            sublength += sub_length(boxes, right--);
            
          if(left <= right)
            sublength += sub_length(boxes, right--);
        }
        
        if(leftlength + sublength + rightlength > length) {
          long restlength = 0;
          long removed = 0;
          size_t op, i;
          
          for(i = prevleft; i <= prevright; ++i) {
            if(is_operand_at(boxes, i))
              ++removed;
          }
          
          for(i = left; i <= right; ++i)
            restlength += sub_length(boxes, i);
            
          op = 0;
          if(prevleft < left) {
            if(is_operand_at(boxes, prevleft))
              op = prevleft;
          }
          else {
            if(is_operand_at(boxes, right + 1))
              op = right + 1;
          }
          
          if(op) {
            if(leftlength + restlength + rightlength < 3 * length / 4) {
              return pmath_expr_set_item(
                       boxes, op,
                       _pmath_shorten_boxes(
                         pmath_expr_get_item(boxes, op),
                         length - leftlength - rightlength - restlength));
            }
            
            if(op == prevleft && leftlength < 3 * length / 4) {
              rightlength = 0;
              right = len;
              
              if(!is_operand_at(boxes, len)) {
                rightlength = sub_length(boxes, len);
                --right;
              }
              
              for(i = prevleft; i < left; ++i) {
                if(is_operand_at(boxes, i))
                  --removed;
              }
              
              boxes = pmath_expr_set_item(
                        boxes, op,
                        _pmath_shorten_boxes(
                          pmath_expr_get_item(boxes, op),
                          length - leftlength - rightlength));
                          
              prevleft = left;
            }
          }
          
          item = pmath_evaluate(
                   pmath_parse_string_args(
                     "ToBoxes(Skeleton(`1`))", "(i)", removed));
                     
          boxes = pmath_expr_set_item(
                    boxes, prevleft,
                    item);
                    
          for(i = prevleft + 1; i <= prevright; ++i) {
            boxes = pmath_expr_set_item(
                      boxes, i,
                      PMATH_UNDEFINED);
          }
          
          return pmath_expr_remove_all(boxes, PMATH_UNDEFINED);
        }
        
        if(prevleft < left) {
          leftlength += sublength;
          prevleft = left;
        }
        else {
          rightlength += sublength;
          prevright = right;
        }
      }
    }
    
    if( pmath_same(item, pmath_System_FrameBox)           ||
        pmath_same(item, pmath_System_FractionBox)        ||
        pmath_same(item, pmath_System_InterpretationBox)  ||
        pmath_same(item, pmath_System_OverscriptBox)      ||
        pmath_same(item, pmath_System_RadicalBox)         ||
        pmath_same(item, pmath_System_SqrtBox)            ||
        pmath_same(item, pmath_System_StyleBox)           ||
        pmath_same(item, pmath_System_SubsuperscriptBox)  ||
        pmath_same(item, pmath_System_TagBox)             ||
        pmath_same(item, pmath_System_UnderoverscriptBox) ||
        pmath_same(item, pmath_System_UnderscriptBox)) 
    {
      for(; len > 0; --len) {
        boxes = pmath_expr_set_item(
                  boxes, len,
                  _pmath_shorten_boxes(
                    pmath_expr_extract_item(boxes, len),
                    length));
      }
      
      return boxes;
    }
  }
  
  return boxes;
}

PMATH_PRIVATE pmath_t builtin_toboxes(pmath_expr_t expr) {
  /* ToBoxes(object)
   */
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  return pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_MAKEBOXES));
}
