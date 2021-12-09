/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2021 Tom Zander <tom@flowee.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "Wallet.h"
#include "Wallet_p.h"
#include "FloweePay.h"

// #define DEBUG_UTXO_SELECTION
#ifdef DEBUG_UTXO_SELECTION
# define logUtxo logCritical
#else
# define logUtxo if (false) logCritical
#endif

namespace {
struct UnspentOutput {
    Wallet::OutputRef outputRef;
    qint64 value = 0;       // in satoshis
    int score = 0;          // the score gained by using this output.
    int walletSecretId = -1;    // the private key that this UTXO is unlocked by
};
}

int Wallet::scoreForSolution(const OutputSet &set, int64_t change, size_t unspentOutputCount) const
{
    assert(unspentOutputCount > 0);
    assert(set.outputs.size() > 0);
    assert(change > 0);

    const int resultingOutputCount = unspentOutputCount - set.outputs.size();
    int score = 0;
    if (m_singleAddressWallet) {
        // aim to keep our output count between 5 and 15
        // Rationale: (in the context of this being a single-address walllet)
        //  there is no loss in privacy in any combination, as such we aim to only
        //  optimize to have no unconfirmed transactions when the user wants to pay.
        if (resultingOutputCount > 8 && resultingOutputCount <= 15)
            score = 1000; // perfection
        else if (resultingOutputCount <= 8)
            score = 800 - (8 - resultingOutputCount) * 200; // for the 0 - 10 range
        else
            score = 500 - (resultingOutputCount - 15) * 40;
    }
    else {
        // aim to keep our output count between 10 and 50
        // Rationale:
        //  To keep privacy we ideally always have a single input and avoid
        //  combining inputs that to the outside world are unconnected.
        //
        //  Additionally, we want to keep enough outputs to avoid reusing
        //  unconfirmed ones.
        if (resultingOutputCount > 10 && resultingOutputCount <= 50)
            score = 1000; // perfection
        else if (resultingOutputCount > 5 && resultingOutputCount < 40)
            score = 250;
        else if (resultingOutputCount < 25 && resultingOutputCount > 10)
            score = 250;
        else if (resultingOutputCount > 40)
            score -= (resultingOutputCount - 40) * 5;
        else if (resultingOutputCount <= 5)
            score -= (5 - resultingOutputCount) * 40; // for the 0 - 5 range
    }

    // in most cases no modifier is added due to change
    if (change < 500)
        score += 2000; // thats very nice (if over 0, that's for the miner)
    else if (change < 2000) // we would create very small UTXO, not nice.
        score -= 1000;
    else if (change < 5000) // ditto
        score -= 800;

    return score;
}

Wallet::OutputSet Wallet::findInputsFor(qint64 output, int feePerByte, int txSize, int64_t &change) const
{
    /*
     * The main selection criterea is the amount of outputs we have afterwards.
     *
     * The goal is to always have between 10 and 15 outputs of varying sizes in
     * our wallet, this makes sure we avoid being without confirmed outputs. Even
     * on medium-heavy usage.
     *
     *
     * As we assume the first items in the list of unspentOutputs are the oldest, all
     * we need to do is find the combination of inputs that works best.
     */

    const int currentBlockHeight = FloweePay::instance()->headerChainHeight();

    QList<UnspentOutput> unspentOutputs;
    std::map<uint64_t, size_t> utxosBySize;
    unspentOutputs.reserve(m_unspentOutputs.size());
    for (auto iter = m_unspentOutputs.begin(); iter != m_unspentOutputs.end(); ++iter) {
        if (m_lockedOutputs.find(iter->first) != m_lockedOutputs.end())
            continue;
        UnspentOutput out;
        out.value = iter->second;
        out.outputRef = OutputRef(iter->first);
        auto wtxIter = m_walletTransactions.find(out.outputRef.txIndex());
        Q_ASSERT(wtxIter != m_walletTransactions.end());

        const auto &wtx = wtxIter->second;
        int h = wtx.minedBlockHeight;
        if (h == WalletPriv::Unconfirmed) {
            out.score = -10; // unconfirmed.
        } else if (wtx.isCoinbase
                   && h + MATURATION_AGE >= m_lastBlockHeightSeen) {
            // don't spend an immature coinbase
            continue;
        } else {
            const int diff = currentBlockHeight - h;
            if (diff > 4024)
                out.score = 0;
            else if (diff > 1008)
                out.score = -10;
            else if (diff > 144)
                out.score = -30;
        }
        utxosBySize.insert(std::make_pair(iter->second, unspentOutputs.size()));
        if (wtx.isCashFusionTx)
            out.score += 50;
        if (!m_singleAddressWallet) {
            const auto outputIter = wtx.outputs.find(out.outputRef.outputIndex());
            assert(outputIter != wtx.outputs.end());
            out.walletSecretId = outputIter->second.walletSecretId; // TODO use
        }
        unspentOutputs.push_back(out);
    }

    // First simply walk from oldest to newest until funded
    OutputSet bestSet;
    int bestScore = 0;
    bestSet.fee = txSize * feePerByte;
    for (auto iter = unspentOutputs.begin(); iter != unspentOutputs.end(); ++iter) {
        bestSet.outputs.push_back(OutputRef(iter->outputRef));
        bestSet.totalSats += iter->value;
        bestSet.fee += BYTES_PER_OUTPUT * feePerByte;
        bestScore += iter->score;

        if (output != -1 && bestSet.totalSats - bestSet.fee >= output)
            break;
    }
    if (output == -1) { // the magic number to return all unspent outputs
        change = 0;
        return bestSet;
    }
    if (bestSet.totalSats - bestSet.fee < output)
        return OutputSet();

    int scoreAdjust = scoreForSolution(bestSet, bestSet.totalSats - bestSet.fee - output,
                                  unspentOutputs.size());
    logUtxo() << "Initial set, size:" << bestSet.outputs.size() << "paying" << bestSet.totalSats << "+"
              << bestSet.fee << "fee, gives change of:" << bestSet.totalSats - bestSet.fee - output
              << "got score" << bestScore << "+" << scoreAdjust;
    bestScore += scoreAdjust;
    // try a new set.
    OutputSet current;
    int score = 0;
    current.fee = txSize * feePerByte;
    for (auto iterBySize = utxosBySize.crbegin(); iterBySize != utxosBySize.crend(); ++iterBySize) {
        const auto &utxo = unspentOutputs.at(iterBySize->second);
        current.outputs.push_back(utxo.outputRef);
        current.totalSats += utxo.value;
        current.fee += BYTES_PER_OUTPUT * feePerByte;
        score += utxo.score;

        if (current.totalSats - current.fee >= output)
            break;
    }

    if (current.totalSats - current.fee >= output) {
        scoreAdjust = scoreForSolution(current, current.totalSats - current.fee - output,
                                  unspentOutputs.size());
        logUtxo() << "Size-based set, size:" << current.outputs.size() << "paying" << current.totalSats << "+"
                  << current.fee << "fee, gives change of:" << current.totalSats - current.fee - output
                  << "got score" << score << "+" << scoreAdjust;
        score += scoreAdjust;

        if (current.outputs.size() > MAX_INPUTS) {
            logUtxo() << "  + Skipping due to too-many-inputs";
        }
        // compare with the cost of oldest to newest.
        else if (score > bestScore) {
            logUtxo() << "  + New BEST";
            bestScore = score;
            bestSet = current;
        }
    }

    // Last we use random sets.
    for (int setIndex = 0; setIndex < 50; ++setIndex) {
        current = OutputSet();
        score = 0;
        current.fee = txSize * feePerByte;
        auto outputs = unspentOutputs;
        do {
            Q_ASSERT(!outputs.empty());
            const int index = static_cast<int>(rand() % outputs.size());
            Q_ASSERT(outputs.size() > index);
            const auto &out = outputs[index];
            current.outputs.push_back(out.outputRef);
            current.totalSats += out.value;
            current.fee += BYTES_PER_OUTPUT * feePerByte;
            score += out.score;

            outputs.removeAt(index); // take it.
        } while (current.totalSats - current.fee < output);

        scoreAdjust = scoreForSolution(current,
                                  (current.totalSats - current.fee) - output, unspentOutputs.size());
        logUtxo() << "Random set, size:" << current.outputs.size() << "paying" << current.totalSats << "+"
                  << current.fee << "fee, gives change of:" << current.totalSats - current.fee - output
                  << "got score" << score << "+" << scoreAdjust;
        score += scoreAdjust;
        Q_ASSERT(current.totalSats - current.fee >= output);
        if (current.outputs.size() > MAX_INPUTS) {
            logUtxo() << "  + Skipping due to too-many-inputs";
        }
        else if (score > bestScore) {
            logUtxo() << "  + New BEST";
            bestScore = score;
            bestSet = current;
        }
    }

    change = bestSet.totalSats - bestSet.fee - output;
    logInfo() << "selected a set with" << bestSet.outputs.size() << "inputs, change:" << change;
    return bestSet;
}
