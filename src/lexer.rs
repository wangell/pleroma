
use crate::common::{String, ToString};
use core::iter::Peekable;

pub type Spanned<Tok, Loc, Error> = Result<(Loc, Tok, Loc), Error>;

#[derive(Debug, Clone)]
pub enum Tok {
    Hu8 { value: u8 },
    Space,
    Colon,
    Semicolon,
    LeftBracket,
    RightBracket,
    Let,
    Equals,

    ReturnKeyword,
    Plus,
    Minus,
    Times,
    Divide,

    LeftParen,
    RightParen,

    IndexLeft,
    IndexRight,

    LeftSquareBracket,
    RightSquareBracket,

    QuotationMark,

    Message,

    Entity,
    Function,
    ReturnType,
    Import,
    Comma,

    Await,
    Print,

    Tab,
    Linefeed,
    EntityIdentifier { value: String },
    StringToken { value: String },
    Symbol { value: String },
    Number,
    Pu32,
    Far,
    Loc,
    Aln,
    Void
}

#[derive(Debug)]
pub enum LexicalError {
    // Not possible
}

use crate::common::str::CharIndices;

pub struct Lexer<'input> {
    chars: Peekable<CharIndices<'input>>,
    next_char: Option<(usize, char)>,
    set_char: bool
}

impl<'input> Lexer<'input> {
    pub fn new(input: &'input str) -> Self {
        Lexer { chars: input.char_indices().peekable(), next_char : Some((1, 'a')), set_char : true }
    }
}

impl<'input> Iterator for Lexer<'input> {
    type Item = Spanned<Tok, usize, LexicalError>;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            if self.set_char {
                self.next_char = self.chars.next();
            }

            match self.next_char {
                Some((i, ';')) => { self.set_char = true; return Some(Ok((i, Tok::Semicolon, i+1))); },

                Some((i, '{')) => { self.set_char = true; return Some(Ok((i, Tok::LeftBracket, i+1)));},
                Some((i, '}')) => { self.set_char = true; return Some(Ok((i, Tok::RightBracket, i+1)))},

                Some((i, '(')) => { self.set_char = true; return Some(Ok((i, Tok::LeftParen, i+1)));},
                Some((i, ')')) => { self.set_char = true; return Some(Ok((i, Tok::RightParen, i+1)))},

                Some((i, 'δ')) => { self.set_char = true; return Some(Ok((i, Tok::Function, i+1)))},

                Some((i, ',')) => { self.set_char = true; return Some(Ok((i, Tok::Comma, i+1)))},
                Some((i, ':')) => { self.set_char = true; return Some(Ok((i, Tok::Colon, i+1)))},
                Some((i, '~')) => { self.set_char = true; return Some(Ok((i, Tok::Import, i+1)))},
                Some((i, 'ε')) => { self.set_char = true; return Some(Ok((i, Tok::Entity, i+1)))},

                Some((i, '=')) => { self.set_char = true; return Some(Ok((i, Tok::Equals, i+1)))},

                Some((i, '+')) => { self.set_char = true; return Some(Ok((i, Tok::Plus, i+1)))},

                Some((i, '!')) => { self.set_char = true; return Some(Ok((i, Tok::Message, i+1)))},

                Some((i, '↵')) => { self.set_char = true; return Some(Ok((i, Tok::ReturnKeyword, i+1)))},

                Some((i, '→')) => { self.set_char = true; return Some(Ok((i, Tok::ReturnType, i+1)))},

                Some((i, ch)) if ch.is_numeric() => {
                    let mut sym = ch.to_string();
                    let start_i = i;
                    let mut last_i = start_i;
                    self.next_char = self.chars.next();

                    loop {
                        let Some((i2, ch2)) = self.next_char else {panic!()};
                        if ch2.is_numeric() {
                            sym.push(ch2);
                            last_i += 1;
                            self.next_char = self.chars.next();
                        } else {
                            self.set_char = false;
                            break;
                        }
                    }

                    let parsed_val : u64 = sym.parse().unwrap();
                    return Some(Ok((start_i, Tok::Hu8 { value: parsed_val.try_into().unwrap() }, last_i)));
                },

                Some((i, ch)) if ch.is_alphabetic() => {
                    let mut sym = ch.to_string();
                    let start_i = i;
                    let mut last_i = start_i;
                    //self.chars.advance_by(1);
                    self.next_char = self.chars.next();

                    loop {
                        let Some((i2, ch2)) = self.next_char else {panic!()};
                        if ch2.is_alphanumeric() {
                            sym.push(ch2);
                            last_i += 1;
                            self.next_char = self.chars.next();
                        } else {
                            self.set_char = false;
                            break;
                        }
                    }

                    match sym.as_str() {
                        "await" => return Some(Ok((start_i, Tok::Await, last_i))),
                        "print" => return Some(Ok((start_i, Tok::Print, last_i))),
                        "loc" => return Some(Ok((start_i, Tok::Loc, last_i))),
                        "far" => return Some(Ok((start_i, Tok::Far, last_i))),
                        "u32" => return Some(Ok((start_i, Tok::Pu32, last_i))),
                        "let" => return Some(Ok((start_i, Tok::Let, last_i))),
                        _ => return Some(Ok((start_i, Tok::Symbol { value: sym }, last_i))),
                    }
                },

                None => return None, // End of file
                _ => { self.set_char = true; continue;}, // Comment; skip this character
            }
        }
    }
}
