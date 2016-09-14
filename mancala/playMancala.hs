-- Leo Gladkov, CMSC 161, lab8

module Main where

import MancalaBoard
import System.IO


playerString :: Player -> String
playerString p = if playerNum p == 0 then "Player A" else "Player B"

winnerString :: MancalaBoard -> String
winnerString board = case winners board of
    [] -> "\nWe have a tie!\n"
    [p] -> "\nCongratulations " ++ playerString p ++ "! You have won!\n"

gameLoop :: MancalaBoard -> IO ()
gameLoop board =
    if gameOver board then putStrLn $ winnerString board
    else do
        putStrLn $ show board
        let p = getCurPlayer board
        let player = playerString p
        putStrLn $ player ++ "'s turn: "
        i <- getLine
        let n = if playerNum p == 0 then read i - 1 else read i + 6
        let board' = move board n
        gameLoop board'

main = do
    putStrLn $ "\nWelcome to Mancala!\nTo play, please enter a number 1-6 " ++
               "\ncorresponding to the pit you want to use."
    gameLoop initial