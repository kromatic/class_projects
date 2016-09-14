-- Leo Gladkov, CMSC 161, lab8

module MancalaBoard (MancalaBoard, Player, initial, getCurPlayer, playerNum,
            getBoardData, numCaptured, move, allowedMoves, isAllowedMove,
            gameOver, winners) where

import Data.List as List -- for List.elemIndex
import Data.Maybe as Maybe -- for List.elemIndex

{-
 - The stones on a Mancala board are simply recorded as a list of Ints.  The
 -  Ints come in the following order:
 - 1. The boardSize pits belonging to PlayerA
 - 2. The store belonging to PlayerA
 - 3. The boardSize pits belonging to PlayerB
 - 4. The store belonging to PlayerB
 -}

data MancalaBoard = MancalaBoardImpl [Int] Player deriving (Eq)

data Player = PlayerA | PlayerB deriving (Eq, Show)

---- Functions/constants for Player ----

allPlayers = [PlayerA, PlayerB]
numPlayers = length allPlayers


playerNum :: Player -> Int
playerNum p = fromJust $ List.elemIndex p allPlayers


playerWithNum :: Int -> Player
playerWithNum i = allPlayers !! i


nextPlayer :: Player -> Player
{- Find the player whose turn is next -}
nextPlayer p = playerWithNum $ ((playerNum p) + 1) `mod` numPlayers


---- Functions/constants for MancalaBoard ----

{- number of pits on each side -}
boardSize = 6
{- number of stones in each pit -}
startStones = 4

{- the initial mancala board -}
initial :: MancalaBoard
initial = MancalaBoardImpl (concat $ take numPlayers (repeat boardSide)) PlayerA
                        -- One side of board                pit at end
    where boardSide = take boardSize (repeat startStones) ++ [0]


{- return the index of the first pit belonging to a player -}
indexForFirstPit :: Player -> Int
indexForFirstPit p = (playerNum p) * (boardSize + 1)


{- return the index of the store for that player -}
indexForPlayerStore :: Player -> Int
indexForPlayerStore p = boardSize + (indexForFirstPit p)


{- return the indices for the pits (without the store) for a player -}
indicesForPlayerSide :: Player -> [Int]
indicesForPlayerSide p = [firstPit .. lastPit] where
    firstPit = indexForFirstPit p
    lastPit = firstPit + boardSize - 1


---- Retrieve information about Mancala Board
{- return the player who has the current turn -}
getCurPlayer :: MancalaBoard -> Player
getCurPlayer (MancalaBoardImpl _ p) = p 

test1 :: Bool
test1 = getCurPlayer initial == PlayerA

{- return the list of all pits in the board -}
getBoardData :: MancalaBoard -> [Int]
getBoardData (MancalaBoardImpl pits _) = pits

test2 :: Bool
test2 = getBoardData initial == [4,4,4,4,4,4,0,4,4,4,4,4,4,0]


{- return the side of the board for a specified player, including the store at
 - the end -}
playerSide :: MancalaBoard -> Player -> [Int]
playerSide b p = map (pits !!) playerIndices where
    pits = getBoardData b
    playerIndices = indicesForPlayerSide p ++ [indexForPlayerStore p]

test3 :: Bool
test3 = playerSide initial PlayerB == [4,4,4,4,4,4,0]


{- return the number of captured pieces in specified player's store -}
numCaptured :: MancalaBoard -> Player -> Int
numCaptured b p = (getBoardData b) !! (indexForPlayerStore p)

test4 :: Bool
test4 = numCaptured initial PlayerA == 0


{- allowedMoves returns a list of valid moves for the current player:
 - ie. the indices of pits which belong to that player, and which contain one
 - or more pieces -}
allowedMoves :: MancalaBoard -> Player -> [Int]
allowedMoves b p = filter (\i -> (getBoardData b) !! i > 0) $ indicesForPlayerSide p

test5 :: Bool
test5 = allowedMoves initial PlayerB == [7..12]

{- check that a move is valid for the current player -}
isAllowedMove :: MancalaBoard -> Int -> Bool
isAllowedMove b = flip elem $ allowedMoves b (getCurPlayer b)

test6 :: Bool
test6 = not . isAllowedMove initial $ 6

{-helper function for replacing ith element of list-}
replace :: [a] -> Int -> a -> [a]
replace [] _ _ = []
replace xs i a = (fst s) ++ (a : drop 1 (snd s)) where
    s = splitAt i xs


{- We number the pits from 0 to 13 (2 players, 6 pits each and 1 store each)
 - This function takes a board and applies the move where the player selects
 - the numbered pit, giving back an updated board after the move -}
move :: MancalaBoard -> Int -> MancalaBoard
move b i
    | (not . isAllowedMove b) i = b
    | otherwise = MancalaBoardImpl boardAfterMove playerNext where
        pits = getBoardData b
        n = pits!!i -- number of stones to place
        j = indexForPlayerStore (nextPlayer p) -- index where stones cannot be placed
        boardAfterMove = placeStones (replace pits i 0) i j n

        placeStones pits i j n
            | next == j = placeStones pits next j n
            | n == 1 = replace pits next (pits!!next + 1) 
            | otherwise = placeStones (replace pits next (pits!!next + 1)) next j (n-1) 
                where next = (i+1) `mod` length pits

        landIndex pits i j n
            | x >= (j-i) `mod` l = (i+x+1) `mod` l
            | otherwise = (i+x) `mod` l
                where x = n `mod` (l - 1)
                      l = length pits

        p = getCurPlayer b
        playerNext
            | landIndex pits i j n == indexForPlayerStore p = p
            | otherwise = nextPlayer p

test7 :: Bool
board = MancalaBoardImpl [0,0,0,0,0,14,1,0,0,0,0,0,0,2] PlayerA
test7 = move board 5 == MancalaBoardImpl [1,1,1,1,1,1,3,1,1,1,1,1,1,2] PlayerA


{- gameOver checks to see if the game is over (i.e. if one player's side of the
 - board is all empty -}
gameOver :: MancalaBoard -> Bool
gameOver b = any (==0) $ fmap (sum . take boardSize . playerSide b) allPlayers

test8 :: Bool
test8 = gameOver board


{- winner returns a list of players who have the top score: there will only be 
 - one in the list if there is a clear winner, and none if it is a draw -}
winners :: MancalaBoard -> [Player]
winners board
    | a > b = [PlayerA]
    | a < b = [PlayerB]
    | otherwise = []
        where results = fmap (sum . playerSide board) allPlayers
              a = results!!0
              b = results!!1

test9 :: Bool
test9 = winners board == [PlayerA]


---- show
paren :: Show a => a -> String
paren a = "( " ++ show a ++ " )"

toString :: MancalaBoard -> Player -> String
toString b p
    | p == PlayerA = foldr (++) [] (fmap paren $ take boardSize (playerSide b p))
    | p == PlayerB = foldr (++) [] (fmap paren $ reverse $ take boardSize (playerSide b p))

instance Show MancalaBoard where
    show b = "\n{ " ++ show (numCaptured b PlayerB) ++ " }" ++ toString b PlayerB ++ " B" ++
             "\n" ++ toString b PlayerA ++ "{ " ++ show (numCaptured b PlayerA) ++ " } A\n"