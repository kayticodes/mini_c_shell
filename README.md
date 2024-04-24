# Mini C Shell

## Introduction

Welcome to Mini C Shell! This is a personal project where I've implemented my own shell in C. Inspired by well-known shells like bash, this shell provides a command-line interface for interacting with your system.

## Features

- Prints an interactive input prompt
- Parses command line input into semantic tokens
- Implements parameter expansion
- Utilizes shell special parameters $$, $?, and $!
- Tilde (~) expansion
- Implements two shell built-in commands: exit and cd
- Executes non-built-in commands using the appropriate exec(3) function
- Implements redirection operators ‘<’ and ‘>’
- Implements the ‘&’ operator to run commands in the background
- Implements custom behavior for SIGINT and SIGTSTP signals

## Usage

To use smallsh, simply compile the source code and run the executable. You'll be greeted with a command prompt where you can enter your commands just like in any other shell.

## Getting Started

Clone this repository and compile the source code using your favorite C compiler. You can then run the smallsh executable to start using the shell.

## Contribution 

Feel free to contribute to this project by submitting pull requests. If you have any ideas for additional features or improvements, don't hesitate to share them!

 ## License 

This project is licensed under the MIT License 

## Acknowledgements 

Thanks to the creators of the Unix process API and signal handling functions, which made this project possible.

 


