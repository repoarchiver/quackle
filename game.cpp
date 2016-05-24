/*
 *  Quackle -- Crossword game artificial intelligence and analysis tool
 *  Copyright (C) 2005-2006 Jason Katz-Brown and John O'Laughlin.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301  USA
 */

#include <iostream>
#include <sstream>

#include "computerplayer.h"
#include "datamanager.h"
#include "enumerator.h"
#include "evaluator.h"
#include "gameparameters.h"
#include "game.h"

// define this to get warnings when there's a problem bag
#define DEBUG_BAG

using namespace Quackle;

Game::Game()
	: m_defaultComputerPlayer(0)
{
	reset();
}

Game::~Game()
{
}

void Game::reset()
{
	m_positions.clear();

	m_title = UVString();
	m_description = UVString();

	PlayerList dummyPlayers;
	dummyPlayers.push_back(Player(MARK_UV("Dummy"), Quackle::Player::HumanPlayerType, 0));
	setPlayers(dummyPlayers);
}

void Game::setPlayers(const PlayerList &list)
{
	PlayerList idedPlayers(list);
	const PlayerList::iterator end(idedPlayers.end());
	int i = 0;
	for (PlayerList::iterator it = idedPlayers.begin(); it != end; ++it, ++i)
		(*it).setId(i);

	m_positions.setPlayers(idedPlayers);
}

void Game::addPosition()
{
	addClonePosition();
	m_positions.lastPosition().incrementTurn();

	m_positions.setCurrentLocation(m_positions.lastLocation());

	// start with move list afresh
	m_positions.lastPosition().removeAllMoves();

	// don't reset if game is over as increment might
	// have set the move made to be an unused tile bonus
	if (!m_positions.lastPosition().gameOver())
		resetCandidature();
}

void Game::addClonePosition()
{
	m_positions.eraseAfter(m_positions.currentLocation());

	if (m_positions.empty())
		m_positions.push_back(GamePosition(m_positions.players()));
	else
		m_positions.push_back(m_positions.lastPosition());
}

void Game::setCurrentPosition(const GamePosition &position)
{
	currentPosition() = position;
	setCurrentPosition(position.location());
}

void Game::associateComputerPlayer(int playerId, ComputerPlayer *computerPlayer)
{
	m_computerPlayers[playerId] = computerPlayer;
}

void Game::associateKnownComputerPlayers()
{
	const PlayerList::const_iterator end(m_positions.players().end());
	for (PlayerList::const_iterator it = m_positions.players().begin(); it != end; ++it)
	{
		if ((*it).type() == Player::ComputerPlayerType)
		{
			ComputerPlayer *computerPlayer = (*it).computerPlayer();

			if (computerPlayer)
				associateComputerPlayer((*it).id(), computerPlayer);
		}
	}
}

ComputerPlayer *Game::computerPlayer(int playerId)
{
	if (m_computerPlayers.find(playerId) == m_computerPlayers.end())
		return defaultComputerPlayer();

	return m_computerPlayers[playerId];
}

void Game::setDefaultComputerPlayer(ComputerPlayer *computerPlayer)
{
	m_defaultComputerPlayer = computerPlayer;
}

ComputerPlayer *Game::defaultComputerPlayer()
{
	if (m_defaultComputerPlayer)
		return m_defaultComputerPlayer;

	m_defaultComputerPlayer = new StaticPlayer;
	return m_defaultComputerPlayer;
}

Move Game::haveComputerPlay(ComputerPlayer *computerPlayer)
{
	if (currentPosition().gameOver())
		return Move::createNonmove();

	if (!computerPlayer)
		computerPlayer = this->computerPlayer(currentPosition().currentPlayer().id());

	computerPlayer->setPosition(currentPosition());

	Move move(computerPlayer->move());
	commitMove(move);
	return move;
}

void Game::advanceToNoncomputerPlayer()
{
	while (!currentPosition().gameOver() && currentPosition().currentPlayer().type() == Player::ComputerPlayerType)
		haveComputerPlay();
}

void Game::commitCandidate(bool maintainBoard)
{
	if (currentPosition().gameOver())
		return;

	currentPosition().prepareForCommit();
	Move moveMade(currentPosition().moveMade());
	addPosition();

	currentPosition().makeMove(moveMade, maintainBoard);
}

void Game::commitMove(const Move &move)
{
	setCandidate(move);
	commitCandidate();
}

///////////

GamePosition::GamePosition(const PlayerList &players)
	: m_players(players), m_currentPlayer(m_players.end()), m_playerOnTurn(m_players.end()), m_turnNumber(0), m_nestedness(0), m_scorelessTurnsInARow(0), m_gameOver(false)
{
	setEmptyBoard();
	resetMoveMade();
	resetBag();
}

GamePosition::GamePosition(const GamePosition &position)
	: m_players(position.m_players), m_moves(position.m_moves), m_moveMade(position.m_moveMade), m_committedMove(position.m_committedMove), m_turnNumber(position.m_turnNumber), m_nestedness(position.m_nestedness), m_scorelessTurnsInARow(position.m_scorelessTurnsInARow), m_gameOver(position.m_gameOver), m_board(position.m_board), m_bag(position.m_bag), m_drawingOrder(position.m_drawingOrder), m_exchangeDividends(position.m_exchangeDividends), m_exchangeDividendIndex(position.m_exchangeDividendIndex), m_explanatoryNote(position.m_explanatoryNote)
{
	// reset iterator
	if (position.turnNumber() == 0)
	{
		m_currentPlayer = m_players.end();
		m_playerOnTurn = m_players.end();
	}
	else
	{
		setCurrentPlayer(position.currentPlayer().id());
		setPlayerOnTurn(position.currentPlayer().id());
	}
}

const GamePosition &GamePosition::operator=(const GamePosition &position)
{
	m_players = position.m_players;
	m_moves = position.m_moves;
	m_moveMade = position.m_moveMade;
	m_committedMove = position.m_committedMove;
	m_turnNumber = position.m_turnNumber;
	m_nestedness = position.m_nestedness;
	m_scorelessTurnsInARow = position.m_scorelessTurnsInARow;
	m_gameOver = position.m_gameOver;
	m_board = position.m_board;
	m_bag = position.m_bag;
	m_drawingOrder = position.m_drawingOrder;
	m_exchangeDividends = position.m_exchangeDividends;
	m_exchangeDividendIndex = position.m_exchangeDividendIndex;
	m_explanatoryNote = position.m_explanatoryNote;

	// reset iterator
	if (position.turnNumber() == 0)
	{
		m_currentPlayer = m_players.end();
		m_playerOnTurn = m_players.end();
	}
	else
	{
		setCurrentPlayer(position.currentPlayer().id());
		setPlayerOnTurn(position.currentPlayer().id());
	}
	return *this;
}

GamePosition::GamePosition()
	: m_currentPlayer(m_players.end()), m_playerOnTurn(m_players.end()), m_turnNumber(0), m_nestedness(0), m_scorelessTurnsInARow(0), m_gameOver(false)
{
	setEmptyBoard();
	resetMoveMade();
	resetBag();
}

void GamePosition::kibitz(int nmoves) {
	/*
	Generator generator(*this);
	generator.kibitz(nmoves, exchangeAllowed() ?
			 Generator::RegularKibitz : Generator::CannotExchange);

	m_moves = generator.kibitzList();

	const MoveList::iterator end(m_moves.end());
	for (MoveList::iterator it = m_moves.begin(); it != end; ++it)
		ensureMovePrettiness(*it);
	*/
}

const Move &GamePosition::staticBestMove()
{
	kibitz(1);
	return m_moves.back();
}

void GamePosition::removeMove(const Quackle::Move &move)
{
	const MoveList::iterator end(m_moves.end());
	for (MoveList::iterator it = m_moves.begin(); it != end; ++it)
	{
		if ((*it) == move)
		{
			m_moves.erase(it);
			break;
		}
	}
}

void GamePosition::removeAllMoves()
{
	m_moves.clear();
}

void GamePosition::ensureMovePrettiness(Move &move)
{
	if (move.prettyTiles().empty())
		move.setPrettyTiles(m_board.prettyTilesOfMove(move));
}

void GamePosition::ensureMoveTilesDoNotIncludePlayThru(Move &move)
{
	move.setTiles(m_board.sanitizedTilesOfMove(move));
}

void GamePosition::addMove(const Move &move)
{
	if (move.isAMove())
	{
		Move prettifiedMove(move);
		ensureMoveTilesDoNotIncludePlayThru(prettifiedMove);
		ensureMovePrettiness(prettifiedMove);
		m_moves.push_back(prettifiedMove);

		MoveList::sort(m_moves, MoveList::Win);
	}
}

void GamePosition::makeSureMoveListContainsMoves(const MoveList &moves)
{
	for (MoveList::const_iterator it = moves.begin(); it != moves.end(); ++it)
		if (!m_moves.contains(*it))
			addMove(*it);
}

void GamePosition::kibitzAs(ComputerPlayer *computerPlayer, int nmoves)
{
	computerPlayer->setPosition(*this);
	setMoves(computerPlayer->moves(nmoves));
}

void GamePosition::addAndSetMoveMade(const Move &move)
{
	addMove(move);
	setMoveMade(move);
}

int GamePosition::validateMove(const Move &move) const
{
	int ret = ValidMove;

	switch (move.action)
	{
	case Move::Place:
	case Move::Exchange:
		if (!currentPlayer().rack().contains(move.usedTiles()))
			ret |= InvalidTiles;

		if (move.action == Move::Place)
		{
			// Place action -- ensure the word connects to others
			if (!isConnected(move))
				ret |= InvalidPlace;

			if (m_board.isUnacceptableOpeningMove(move))
				ret |= InvalidOpeningPlace;

			// check if the word is acceptable
			if (!formsAcceptableWords(move))
				ret |= UnacceptableWord;
		}
		else
		{
			// Exchange action -- ensure there are enough tiles in bag
			if (!exchangeAllowed())
				ret |= TooLateExchange;
		}
		break;

	case Move::UnusedTilesBonus:
	case Move::TimePenalty:
		ret = InvalidAction;
		break;

	case Move::Pass:
	case Move::Nonmove:
		break;
	}

	return ret;
}

bool GamePosition::isConnected(const Move &move) const
{
	return m_board.isConnected(move);
}

bool GamePosition::formsAcceptableWords(const Move &move) const
{
	vector<Move> words = m_board.allWordsFormedBy(move);

	for (vector<Move>::const_iterator it = words.begin(); it != words.end(); ++it)
	{
		if (!isAcceptableWord((*it).wordTiles()))
			return false;
	}

	return true;
}

MoveList GamePosition::allWordsFormedBy(const Move &move) const
{
    return m_board.allWordsFormedBy(move);
}

bool GamePosition::isAcceptableWord(const LetterString &word) const
{
	return false;
	//Generator generator;
	//return generator.isAcceptableWord(word);
}

bool GamePosition::exchangeAllowed() const
{
	return m_bag.size() >= QUACKLE_PARAMETERS->minimumTilesForExchange();
}

int GamePosition::handleOverdraw(const LetterString &letters, LetterString *throwback) const
{
	if (!unseenBag().removeLetters(letters))
		return OverdrawnTilesNotUnseen;

	if (letters.size() <= QUACKLE_PARAMETERS->overdrawPenalty())
		return InvalidOverdrawNumber;

	ProbableRackList racks;
	Bag bag(letters);
	Enumerator enumerator(bag);
	enumerator.enumerate(&racks, QUACKLE_PARAMETERS->overdrawPenalty());

	double minimumLeaveValue = 99999;

	for (ProbableRackList::const_iterator it = racks.begin(); it != racks.end(); ++it)
	{
		double value = leaveValue((*it).rack.tiles());

		if (value < minimumLeaveValue)
		{
			Rack rack(letters);
			rack.unload((*it).rack.tiles());
			*throwback = (Rack(letters) - (*it).rack).tiles();
			minimumLeaveValue = value;
		}
	}
	return ValidOverdraw;
}

void GamePosition::makeMove(const Move &move, bool maintainBoard)
{
	if (!move.isChallengedPhoney())
	{
		//Generator generator(*this);
		//generator.makeMove(move, maintainBoard);
		//m_board = generator.position().board();
		m_board.makeMove(move);
	}

	if (move.action == Move::Exchange) {
		m_bag.toss(move.usedTiles());
		if (!m_exchangeDividends.empty()) {
			for (const Letter& tile : move.usedTiles()) {
				const int exchangeDividendIndex =
					m_exchangeDividendIndex % m_exchangeDividends.size();
				const int exchangeDividend = m_exchangeDividends[exchangeDividendIndex];
				//UVcout << "exchangeDividend: " << exchangeDividend << endl;
				const int pos = exchangeDividend % (m_drawingOrder.size() + 1);
				//UVcout << "inserting at position: " << pos << endl;
				m_drawingOrder = m_drawingOrder.insert(pos, 1, static_cast<char>(tile));
				// UVcout << "new drawingOrder: "
				// 			 << QUACKLE_ALPHABET_PARAMETERS->userVisible(m_drawingOrder)
				// 			 << endl;
			}
		}
	}
}

void GamePosition::scoreMove(Move &move)
{
	move.score = calculateScore(move);
	move.equity = calculateEquity(move);
}

void GamePosition::ensureBoardIsPreparedForAnalysis()
{
	//Generator generator(*this);
	//generator.allCrosses();
	//m_board = generator.position().board();
}

int GamePosition::calculateScore(const Move &move)
{
	return m_board.score(move);
}

double GamePosition::calculateEquity(const Move &move)
{
	return QUACKLE_EVALUATOR->equity(*this, move);
}

double GamePosition::calculatePlayerConsideration(const Move &move)
{
	return QUACKLE_EVALUATOR->playerConsideration(*this, move);
}

double GamePosition::leaveValue(const LetterString &leave) const
{
	return QUACKLE_EVALUATOR->leaveValue(leave);
}

double GamePosition::calculateSharedConsideration(const Move &move)
{
	return QUACKLE_EVALUATOR->sharedConsideration(*this, move);
}

Bag GamePosition::unseenBag() const
{
	return unseenBagFromPlayerPerspective(currentPlayer());
}

Bag GamePosition::unseenBagFromPlayerPerspective(const Player &player) const
{
	// one way:
	//Bag ret(m_board.tilesNotOnBoard());
	//ret.removeLetters(currentPlayer().rack().usedTiles());

	// other way:
	Bag ret(m_bag);
	const PlayerList::const_iterator end(m_players.end());
	for (PlayerList::const_iterator it = m_players.begin(); it != end; ++it)
	{
		if (!((*it) == player))
		{
			ret.toss((*it).rack());
		}
	}

	return ret;
}

void GamePosition::ensureProperBag() const
{
#ifdef DEBUG_BAG
	Bag allTiles(m_board.tilesOnBoard());

	UVString debugString;

	Bag racks;
	racks.clear();

	const PlayerList::const_iterator end(m_players.end());
	for (PlayerList::const_iterator it = m_players.begin(); it != end; ++it)
	{
		allTiles.toss((*it).rack());
		racks.toss((*it).rack());
	}

	allTiles.toss(m_bag.shuffledTiles());

	Bag fullDistribution;

	bool mismatch = false;

	char allTilesCounts[QUACKLE_FIRST_LETTER + QUACKLE_MAXIMUM_ALPHABET_SIZE];
	char fullCounts[QUACKLE_FIRST_LETTER + QUACKLE_MAXIMUM_ALPHABET_SIZE];
	allTiles.letterCounts(allTilesCounts);
	fullDistribution.letterCounts(fullCounts);

	for (Letter letter = 0; letter <= QUACKLE_ALPHABET_PARAMETERS->lastLetter(); ++letter)
	{
		if (allTilesCounts[(int)(letter)] != fullCounts[(int)(letter)])
		{
			mismatch = true;
			if (debugString.empty()) {
			    debugString += MARK_UV("-------------------------\n");
			    debugString += MARK_UV("tiles on board: ");
			    debugString += m_board.tilesOnBoard().toString() + MARK_UV("\n");
			    debugString += MARK_UV("racks: ") + racks.toString() + MARK_UV("\n");
			    debugString += MARK_UV("bag: ") + m_bag.toString() + MARK_UV("\n");
			    debugString += MARK_UV("All tiles: ") + allTiles.toString() + MARK_UV("\n");
			}
			debugString += MARK_UV("Letter doesn't match: ") + QUACKLE_ALPHABET_PARAMETERS->userVisible(letter) + MARK_UV("\n");
		}
	}
	if (mismatch)
	{
		UVcout << "********* PROBLEM PROBLEM *********" << endl;
		UVcout << debugString;
		UVcout << "-------------------------" << endl;
	}
#endif
}

void GamePosition::setEmptyBoard()
{
	m_board.prepareEmptyBoard();
}

const Player &GamePosition::humanPlayer() const
{
	if (currentPlayer().type() == Player::HumanPlayerType)
		return currentPlayer();
	bool found;
	return *nextPlayerOfType(Player::HumanPlayerType, found);
}

void GamePosition::replenishAndSetRack(const Rack &previousRack, Player &player)
{
#ifdef VERBOSE_DEBUG_BAG
	UVcout << "replenishAndSetRack(" << previousRack << ", " << player << ")" << endl;
#endif
	Rack newRack(previousRack);

#ifdef VERBOSE_DEBUG_BAG
	UVcout << "(Before refill) " << m_bag << endl;
#endif

	if (m_drawingOrder.empty())
		m_bag.refill(newRack);
	else
		m_drawingOrder = m_bag.refill(newRack, m_drawingOrder);

	player.setRack(newRack);
	player.setDrawnLetters(newRack - previousRack);

#ifdef VERBOSE_DEBUG_BAG
	UVcout << "(After refill) " << m_bag << endl;
	UVcout << "Rack refilled to " << newRack << endl;
#endif
}

void GamePosition::setCurrentPlayerRack(const Rack &rack, bool adjustBag)
{
	setPlayerRack(currentPlayer().id(), rack, adjustBag);
}

UVString GamePosition::nestednessIndentation() const
{
	UVString ret;
	for (unsigned int i = 0; i < m_nestedness; ++i)
		ret += MARK_UV("\t");
	return ret;
}

void GamePosition::setOppRack(const Rack &rack, bool adjustBag)
{
	int oppID;
	const PlayerList::iterator end(m_players.end());
	for (PlayerList::iterator it = m_players.begin(); it != end; ++it)
		if ((*it).id() != currentPlayer().id())
		{
			m_bag.toss((*it).rack());
			(*it).setRack(Rack(""));
			oppID = (*it).id();
			setPlayerRack(oppID, rack, adjustBag);
			return;
		}
}

// TODO(jasonkb): this function is poorly implemented. Also it should return a reference.
Rack GamePosition::oppRack()
{
	//UVcout << "currentPlayer(): " << currentPlayer() << endl;

	const PlayerList::iterator end(m_players.end());
	/*
	for (PlayerList::iterator it = m_players.begin(); it != end; ++it)
		UVcout << "rack " << *it << " " << (*it).rack() << endl;
	*/
	for (PlayerList::iterator it = m_players.begin(); it != end; ++it)
	{
		if ((*it).id() != currentPlayer().id())
		{
			return (*it).rack();
		}
	}

	return Rack();
}

void GamePosition::setPlayerRack(int playerID, const Rack &rack, bool adjustBag)
{
	const PlayerList::iterator end(m_players.end());
	for (PlayerList::iterator it = m_players.begin(); it != end; ++it)
	{
		if ((*it).id() == playerID)
		{
			// restore bag
			if (adjustBag)
			{
				// please be correct code
				m_bag.toss((*it).rack() - rack);
				removeLetters((rack - (*it).rack()).tiles());
			}

			(*it).setRack(rack);
		}
	}
}

bool GamePosition::canSetCurrentPlayerRackWithoutBagExpansion(const Rack &rack) const
{
	return canSetPlayerRackWithoutBagExpansion(currentPlayer().id(), rack);
}

bool GamePosition::canSetPlayerRackWithoutBagExpansion(int playerID, const Rack &rack) const
{
        (void) playerID;
	Bag someTiles(m_bag);

	const PlayerList::const_iterator end(m_players.end());
	for (PlayerList::const_iterator it = m_players.begin(); it != end; ++it)
		someTiles.toss((*it).rack());

	// Now we have a bag with all tiles not on the board
	return someTiles.removeLetters(rack.tiles());
}

bool GamePosition::setCurrentPlayer(int playerID)
{
	const PlayerList::iterator end(m_players.end());
	for (PlayerList::iterator it = m_players.begin(); it != end; ++it)
	{
		if ((*it).id() == playerID)
		{
			m_currentPlayer = it;
			return true;
		}
	}

	m_currentPlayer = m_players.begin();
	return false;
}

bool GamePosition::setPlayerOnTurn(int playerID)
{
	const PlayerList::iterator end(m_players.end());
	for (PlayerList::iterator it = m_players.begin(); it != end; ++it)
	{
		if ((*it).id() == playerID)
		{
			m_playerOnTurn = it;
			return true;
		}
	}

	m_playerOnTurn = m_players.begin();
	return false;
}

void GamePosition::prepareForCommit()
{
	setCommittedMove(moveMade());
}

Board GamePosition::boardAfterMoveMade() const
{
	Board ret(m_board);
	ret.makeMove(m_moveMade);
	return ret;
}

void GamePosition::resetMoveMade()
{
	m_moveMade = Move::createNonmove();
	m_committedMove = Move::createNonmove();
}

void GamePosition::resetBag()
{
	m_bag.prepareFullBag();
}

bool GamePosition::incrementTurn()
{
	if (gameOver() || m_players.empty())
		return false;

	PlayerList::iterator previousCurrentPlayer(m_currentPlayer);

	bool ret = false;

	// if current player is players.end(), we haven't started game
	// and go to first player after incrementing
	if (m_currentPlayer != m_players.end())
	{
		Rack remainingRack(currentPlayer().rack() - m_moveMade);

		Rack moveTiles(m_moveMade.usedTiles());
		moveTiles.unload(currentPlayer().rack().tiles());

		// now moveTiles is the tiles that are in play but not on rack
		removeLetters(moveTiles.tiles());

		// update our current player's score before possibly
		// adding endgame bonuses
		currentPlayer().addToScore(m_moveMade.effectiveScore());

		// player played out
		if (m_bag.empty() && remainingRack.empty())
		{
			// magic!
			++m_turnNumber;

			if (!m_gameOver)
			{
				// we become over based on player playing out
				// with empty bag
				adjustScoresToFinishGame();

				m_gameOver = true;
			}
		}

		if ((m_moveMade.action == Move::Place && m_moveMade.effectiveScore() == 0) || m_moveMade.action == Move::Exchange || m_moveMade.action == Move::Pass)
			++m_scorelessTurnsInARow;
		else
			m_scorelessTurnsInARow = 0;

		if (!m_gameOver)
		{
			if (QUACKLE_PARAMETERS->numberOfScorelessTurnsThatEndsGame() >= 0 && m_scorelessTurnsInARow >= QUACKLE_PARAMETERS->numberOfScorelessTurnsThatEndsGame() && !m_board.isEmpty())
			{
				// magic!
				++m_turnNumber;

				adjustScoresToFinishPassedOutGame();

				m_gameOver = true;
			}
		}

		// refill rack of player that just played
		replenishAndSetRack(remainingRack);

		// now move onto next player
		++m_currentPlayer;
	}

	if (m_currentPlayer == m_players.end())
	{
		++m_turnNumber;
		m_currentPlayer = m_players.begin();
		ret = true;
	}

	// ensure players get a rack at the beginning of the game,
	// and that the first player draws first (even though yes
	// olaugh I realize that it doesn't matter :-)
	if (currentPlayer().rack().empty())
		replenishAndSetRack(currentPlayer().rack());

	// refill rack of any player whose rack is empty (again for
	// beginning of game)
	const PlayerList::iterator end(m_players.end());
	for (PlayerList::iterator it = m_players.begin(); it != end; ++it)
		if ((*it).rack().empty())
			replenishAndSetRack((*it).rack(), *it);

	// freeze current player as last person who played out
	if (gameOver())
		m_currentPlayer = previousCurrentPlayer;

	m_playerOnTurn = m_currentPlayer;

	// kill drawing order
	//m_drawingOrder = "";
	//m_exchangeDividends.clear();
	
	// reset note to a clean slate
	m_explanatoryNote = UVString();

	return ret;
}

bool GamePosition::removeLetters(const LetterString &letters)
{
	bool ret = true;

	const LetterString::const_iterator end(letters.end());
	for (LetterString::const_iterator it = letters.begin(); it != end; ++it)
	{
#ifdef VERBOSE_DEBUG_BAG
		UVcout << "removeLetters processing " << QUACKLE_ALPHABET_PARAMETERS->userVisible(*it) << endl;
#endif

		const bool removedFromBag = m_bag.removeLetter(*it);
		if (removedFromBag)
		{
#ifdef VERBOSE_DEBUG_BAG
			UVcout << "removed from bag" << endl;
#endif
		}
		else
		{
			bool removedFromPlayer = false;
			const PlayerList::iterator playersEnd(m_players.end());
			for (PlayerList::iterator playerIt = m_players.begin(); playerIt != playersEnd; ++playerIt)
			{
				if (*playerIt == currentPlayer())
					continue;

				LetterString letterString;
				letterString += (*it);

				Rack newRack((*playerIt).rack());
				removedFromPlayer = newRack.unload(letterString);
				if (removedFromPlayer)
				{
					replenishAndSetRack(newRack, *playerIt);
					break;
				}
			}

			if (removedFromPlayer)
			{
#ifdef VERBOSE_DEBUG_BAG
				UVcout << "removed from player" << endl;
#endif
			}
			else
			{
#ifdef VERBOSE_DEBUG_BAG
				UVcout << "couldn't find any place to remove " << QUACKLE_ALPHABET_PARAMETERS->userVisible(*it) << " from!" << endl;
#endif

				ret = false;
			}
		}
	}

	return ret;
}

PlayerList::const_iterator GamePosition::nextPlayer() const
{
	PlayerList::const_iterator ret = m_currentPlayer;
	ret++;
	if (ret == m_players.end())
		ret = m_players.begin();
	return ret;
}

PlayerList::const_iterator GamePosition::playerWithId(int id, bool &found) const
{
	PlayerList::const_iterator it = m_currentPlayer;
	for (++it; ; ++it)
	{
		if (it == m_players.end())
			it = m_players.begin();

		if ((*it).id() == id)
		{
			found = true;
			return it;
		}

		if (it == m_currentPlayer)
		{
			found = false;
			return it;
		}
	}
}

PlayerList::const_iterator GamePosition::nextPlayerOfType(Player::PlayerType type, bool &found) const
{
	PlayerList::const_iterator ret = m_currentPlayer;
	++ret;
	while (true)
	{
		if (ret == m_players.end())
			ret = m_players.begin();

		if ((*ret).type() == type)
			break;

		if (ret == m_currentPlayer)
		{
			found = false;
			break;
		}

		++ret;
	}

	found = true;
	return ret;
}

PlayerList GamePosition::endgameAdjustedScores() const
{
	PlayerList ret;
	const PlayerList::const_iterator end(m_players.end());
	for (PlayerList::const_iterator it = m_players.begin(); it != end; ++it)
	{
		Player adjustedPlayer(*it);

		if (gameOver() && adjustedPlayer == currentPlayer())
		{
			adjustedPlayer.addToScore(m_moveMade.effectiveScore());
		}

		ret.push_back(adjustedPlayer);
	}

	return ret;
}

PlayerList GamePosition::leadingPlayers() const
{
	PlayerList ret;

	PlayerList players(endgameAdjustedScores());
	for (PlayerList::const_iterator it = players.begin(); it != players.end(); ++it)
	{
		if (!ret.empty() && (*it).score() > ret.back().score())
			ret.clear();

		if (ret.empty() || (*it).score() >= ret.back().score())
			ret.push_back(*it);
	}

	return ret;
}

int GamePosition::spread(int playerID) const
{
	int nextBest = 0;
	int currentPlayerScore = 0;

	PlayerList players(endgameAdjustedScores());
	for (PlayerList::const_iterator it = players.begin(); it != players.end(); ++it)
	{
		if ((*it).id() == playerID)
		{
			currentPlayerScore = (*it).score();
			continue;
		}

		if ((*it).score() > nextBest)
			nextBest = (*it).score();
	}

	return currentPlayerScore - nextBest;
}

void GamePosition::adjustScoresToFinishPassedOutGame()
{
	const PlayerList::iterator end(m_players.end());
	for (PlayerList::iterator it = m_players.begin(); it != end; ++it)
	{
		if ((*it) == currentPlayer())
		{
			m_moveMade = Move::createUnusedTilesBonus((*it).rack().tiles(), -(*it).rack().score());
			m_committedMove = m_moveMade;
		}
		else
		{
			(*it).addToScore(-(*it).rack().score());
		}
	}
}

void GamePosition::adjustScoresToFinishGame()
{
	int addend = 0;
	LetterString clobberedTiles;

	addend = deadwood(&clobberedTiles);

	m_moveMade = Move::createUnusedTilesBonus(clobberedTiles, addend);
	m_committedMove = m_moveMade;
}

int GamePosition::deadwood(LetterString *tiles) const
{
	int addend = 0;
	tiles->clear();

	const PlayerList::const_iterator end(m_players.end());
	for (PlayerList::const_iterator it = m_players.begin(); it != end; ++it)
	{
		if ((*it) == currentPlayer())
			continue;

		addend += (*it).rack().score();
		*tiles += (*it).rack().tiles();
	}

	return addend * 2;
}

bool GamePosition::doesMoveEndGame(const Move &move) const
{
	return (currentPlayer().rack() - move).empty() && m_bag.empty();
}

UVOStream& operator<<(UVOStream& o, const Quackle::GamePosition &position)
{
	o << "------------" << endl;
	o << "Position [turnNumber = " << position.turnNumber() << ", current player " << position.currentPlayer() << endl;
	o << position.board() << endl;
	o << "Move made is " << position.moveMade() << endl;
	o << "All Players: " << endl;
	PlayerList players(position.players());
	const PlayerList::const_iterator end(players.end());
	for (PlayerList::const_iterator it = players.begin(); it != end; ++it)
		o << *it << endl;
	o << position.bag() << endl;
	if (position.gameOver())
		o << "Game over." << endl;
	o << "------------" << endl;
    return o;
}

UVOStream& operator<<(UVOStream& o, const Quackle::PositionList &positions)
{
	const Quackle::PositionList::const_iterator end(positions.end());
	for (Quackle::PositionList::const_iterator it = positions.begin(); it != end; ++it)
		o << *it << endl;
	return o;
}

///////////////

int History::maximumTurnNumber() const
{
	if (empty())
		return 0;

	return back().turnNumber();
}

PositionList History::positionsFacedBy(int playerID) const
{
	PositionList ret;
	const PositionList::const_iterator ourEnd(end());
	for (PositionList::const_iterator it = begin(); it != ourEnd; ++it)
		if ((*it).playerOnTurn().id() == playerID)
			ret.push_back(*it);

	return ret;
}

const GamePosition &History::positionAt(const HistoryLocation &location, bool *exists) const
{
	for (PositionList::const_reverse_iterator it = rbegin(); it != rend(); ++it)
	{
		if ((*it).playerOnTurn().id() == location.playerId() && (*it).turnNumber() == location.turnNumber())
		{
			if (exists) *exists = true;
			return *it;
		}
	}

	if (exists) *exists = false;
	return back();
}

GamePosition &History::mutablePositionAt(const HistoryLocation &location, bool *exists)
{
	for (PositionList::reverse_iterator it = rbegin(); it != rend(); ++it)
	{
		if ((*it).playerOnTurn().id() == location.playerId() && (*it).turnNumber() == location.turnNumber())
		{
			if (exists) *exists = true;
			return *it;
		}
	}

	if (exists) *exists = false;
	return back();
}

const GamePosition &History::nextPosition(bool *exists) const
{
	bool found = false;
	for (PositionList::const_iterator it = begin(); it != end(); ++it)
	{
		if (found)
		{
			if (exists) *exists = true;
			return *it;
		}

		if ((*it).playerOnTurn().id() == m_currentLocation.playerId() && (*it).turnNumber() == m_currentLocation.turnNumber())
		{
			found = true;
		}
	}

	if (exists) *exists = false;
	return back();
}

const GamePosition &History::previousPosition(bool *exists) const
{
	bool found = false;
	for (PositionList::const_reverse_iterator it = rbegin(); it != rend(); ++it)
	{
		if (found)
		{
			if (exists) *exists = true;
			return *it;
		}

		if ((*it).playerOnTurn().id() == m_currentLocation.playerId() && (*it).turnNumber() == m_currentLocation.turnNumber())
		{
			found = true;
		}
	}

	if (exists) *exists = false;
	return back();
}

const GamePosition &History::nextPositionFacedBy(int playerID, bool *exists) const
{
	bool found = false;
	for (PositionList::const_iterator it = begin(); it != end(); ++it)
	{
		if (found && (*it).playerOnTurn().id() == playerID)
		{
			if (exists) *exists = true;
			return *it;
		}

		if ((*it).playerOnTurn().id() == m_currentLocation.playerId() && (*it).turnNumber() == m_currentLocation.turnNumber())
		{
			found = true;
		}
	}

	if (exists) *exists = false;
	return back();
}

const GamePosition &History::firstPosition(bool *exists) const
{
	if (empty())
	{
		if (exists) *exists = false;
		return back();
	}

	if (exists) *exists = true;
	return front();
}

void History::eraseAfter(const HistoryLocation &location)
{
	while (true)
	{
		if (empty())
			break;

		bool isAfter;

		if (location.turnNumber() == back().turnNumber())
			isAfter = location.playerId() < back().playerOnTurn().id();
		else
			isAfter = location.turnNumber() < back().turnNumber();

		if (isAfter)
			pop_back();
		else
			break;
	}
}

////////

HistoryLocation::HistoryLocation(int playerId, int turnNumber)
	: m_playerId(playerId), m_turnNumber(turnNumber)
{
}

bool Quackle::operator<(const HistoryLocation &hl1, const HistoryLocation &hl2)
{
	if (hl1.turnNumber() != hl2.turnNumber())
		return hl1.turnNumber() < hl2.turnNumber();
	return hl1.playerId() < hl2.playerId();
}

bool operator==(const Quackle::HistoryLocation &hl1, const Quackle::HistoryLocation &hl2)
{
	return hl1.turnNumber() == hl2.turnNumber() && hl1.playerId() == hl2.playerId();
}

UVOStream& operator<<(UVOStream& o, const Quackle::HistoryLocation &historyLocation)
{
	o << "location turn number " << historyLocation.turnNumber() << ", player id " << historyLocation.playerId();
	return o;
}

