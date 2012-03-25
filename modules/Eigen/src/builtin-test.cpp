#include "classification.h"
#include "conversion.h"

using namespace pmath;
using namespace pmath4eigen;
using namespace Eigen;


static void print(const char *fmtstr) {
  Evaluate(Call(Symbol(PMATH_SYMBOL_PRINT), String(fmtstr)));
}

static void print(const char *fmtstr, Expr arg1) {
  Evaluate(
    Call(
      Symbol(PMATH_SYMBOL_PRINT),
      Call(
        Symbol(PMATH_SYMBOL_STRINGFORM),
        String(fmtstr),
        arg1)));
}

static void print(const char *fmtstr, Expr arg1, Expr arg2) {
  Evaluate(
    Call(
      Symbol(PMATH_SYMBOL_PRINT),
      Call(
        Symbol(PMATH_SYMBOL_STRINGFORM),
        String(fmtstr),
        arg1,
        arg2)));
}

static Expr matrix_form(Expr m){
  return Call(Symbol(PMATH_SYMBOL_MATRIXFORM), m);
}


template<typename MatrixType>
static void test(Expr matrix, size_t rows, size_t cols){
  MatrixType eigen_matrix(rows, cols);
  
  Converter::to_eigen(eigen_matrix, matrix);
  
  eigen_matrix*= 3;
  
  Expr back = Converter::from_eigen(eigen_matrix);
  
  print("multiplied with 3: `1`", matrix_form(back));
}


pmath_t p4e_builtin_test(pmath_expr_t _expr) {
  Expr expr(_expr);
  
  Expr matrix = expr[1];
  
  size_t rows, cols;
  if(!MatrixKind::get_matrix_dimensions(matrix, rows, cols)) {
    print("`1` is not a matrix.", matrix);
    return PMATH_NULL;
  }
  
  print("dimensions: rows=`1`, cols=`2`", rows, cols);
  
  MatrixKind::Type type = MatrixKind::classify(matrix);
  
  switch(type) {
    case MatrixKind::General:
      print("classified as General");
      test<MatrixXa>(matrix, rows, cols);
      break;
      
    case MatrixKind::MachineReal:
      print("classified as MachineReal");
      test<MatrixXd>(matrix, rows, cols);
      break;
      
    case MatrixKind::MachineComplex:
      print("classified as MachineComplex");
      test<MatrixXcd>(matrix, rows, cols);
      break;
      
    default:
      print("unexpected classification `1`", (int)type);
      break;
  }
  
  return PMATH_NULL;
}
