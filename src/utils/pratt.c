/**
 * The grammar of Lreng in BNF
 * <expression> ::= <identifier>
 *          | <literal>
 *          | "(" <expression> ")"
 *          | "{" <expression> "}"
 *          | "[" <expression> "]"
 *          | <expression> <binary_operator> <expression>
 *          | <unary_operator> <expression>
 *          | <identifier> "(" <expression> ")"
 */
