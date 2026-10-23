options gen2
options stack = 1000000

require peg/peg

def parse_add(input : string; blk : block<(val : int; err : array<ParsingError>) : void>) {
    parse(input) {
        var expr : int
        rule(term as a, WS, "+", WS, term as b, EOF) {
            return a + b
        }
        rule(term as a, EOF) {
            return a
        }
        var term : int
        rule(number as n) {
            return n
        }
    }
}

[export]
def eval_add(input : string) : int {
    var res = -1
    parse_add(input) $(val : int; err : array<ParsingError>) {
        res = length(err) == 0 ? val : -1
    }
    return res
}
