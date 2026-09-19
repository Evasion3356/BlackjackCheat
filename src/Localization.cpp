#include "Localization.h"
#include "Config.h"
#include "Log.h"
#include "script.h" // LANGUAGE::_GET_CURRENT_LANGUAGE_ID() (natives.h, via script.h)

#include <string>
#include <string_view>

namespace
{
	constexpr int kLanguageCount = static_cast<int>(Localization::Language::Count);

	// HIT/STAND/DOUBLE/SPLIT -- the standalone advice readout (see
	// DrawAdviceStatus() in BlackjackCheat.cpp). Kept as short, punchy
	// all-caps imperatives in every language, matching the English
	// original's tone (a HUD command, not a sentence). One row per
	// Language (see Localization.h's enum, same ordering), 4 columns
	// matching BlackjackHandEval::Action's declaration order (Hit,
	// Stand, Double, Split). Verified against real casino/gambling
	// glossaries per language (web search, session after the initial
	// LLM-assisted pass -- see docs/JOURNAL.md) rather than trusted on
	// LLM instinct alone; fr-FR/it-IT/es-ES/es-MX/pt-BR/ko-KR/zh-TW/
	// zh-CN/ja-JP all matched real sources exactly as originally
	// written. Two corrections came out of that pass: de-DE's Stand
	// term was wrong (STEHEN is the term actual German casino/rules
	// pages use, not the originally-guessed HALTEN) and ru-RU's Stand
	// term was wrong (ХВАТИТ is the term Russian sources use, not the
	// originally-guessed СТОП). Still not reviewed by a native speaker,
	// and pl-PL's terms are a reasonable-looking but not source-matched
	// guess (PODWÓJ/PODZIEL are imperative forms of the sourced nouns
	// "podwojenie"/"podział" -- not independently confirmed as the
	// actual imperative a Polish table would use) -- fix a row directly
	// here if a wording turns out to be wrong.
	constexpr int kActionLabelCount = 4;
	constexpr std::string_view kActionLabels[kLanguageCount][kActionLabelCount] =
	{
		{ "HIT", "STAND", "DOUBLE", "SPLIT" },                     // en-US
		{ "TIRER", "RESTER", "DOUBLER", "SÉPARER" },                // fr-FR -- confirmed against regles.com/le-black-jack.com
		{ "ZIEHEN", "STEHEN", "VERDOPPELN", "TEILEN" },             // de-DE -- STEHEN confirmed (not HALTEN) against German casino/rules sources
		{ "CARTA", "STARE", "RADDOPPIA", "DIVIDI" },                // it-IT -- confirmed against it.blackjackinfo.com/pokerstars.it
		{ "PEDIR", "PLANTARSE", "DOBLAR", "DIVIDIR" },              // es-ES -- confirmed against casino.org/es, crehana.com
		{ "PEDIR CARTA", "PARAR", "DOBRAR", "DIVIDIR" },            // pt-BR -- confirmed against pt.pokernews.com, lance.com.br
		{ "DOBIERZ", "STÓJ", "PODWÓJ", "PODZIEL" },                 // pl-PL -- Hit/Stand confirmed (holdemshop.pl); Double/Split are un-sourced imperative forms of confirmed nouns
		{ "ЕЩЕ", "ХВАТИТ", "УДВОИТЬ", "РАЗДЕЛИТЬ" },                // ru-RU -- ХВАТИТ confirmed (not СТОП) against gipsyteam.ru/casino.ru
		{ "히트", "스탠드", "더블", "스플릿" },                        // ko-KR -- confirmed against reviewland.net (loanwords, used as-is at real tables)
		{ "要牌", "停牌", "加倍", "分牌" },                          // zh-TW -- confirmed against baike.baidu.com/zhihu.com
		{ "ヒット", "スタンド", "ダブル", "スプリット" },              // ja-JP -- confirmed against ja.wikipedia.org/majandofu.com
		{ "PEDIR", "PLANTARSE", "DOBLAR", "DIVIDIR" },              // es-MX -- same as es-ES, no meaningful regional difference for these terms
		{ "要牌", "停牌", "加倍", "分牌" },                          // zh-CN
	};

	// BET LOW/MEDIUM/HIGH -- the betting-advice readout (see
	// DrawBettingAdviceStatus()). Column order matches
	// BlackjackHandEval::BettingConfidence's declaration order (Low,
	// Medium, High). This is this mod's OWN invented HUD concept, not a
	// real casino term -- no gambling glossary has a "BET LOW/MEDIUM/
	// HIGH" phrase to verify against, so this table only got a grammar/
	// diacritics pass (restoring accents an earlier ASCII-safety pass
	// had stripped, now that BlackjackCheat.vcxproj carries /utf-8),
	// not the same source-verification kActionLabels above got.
	constexpr int kBettingConfidenceLabelCount = 3;
	constexpr std::string_view kBettingConfidenceLabels[kLanguageCount][kBettingConfidenceLabelCount] =
	{
		{ "BET LOW", "BET MEDIUM", "BET HIGH" },                          // en-US
		{ "MISE FAIBLE", "MISE MOYENNE", "MISE ÉLEVÉE" },                  // fr-FR
		{ "NIEDRIG SETZEN", "MITTEL SETZEN", "HOCH SETZEN" },              // de-DE
		{ "PUNTATA BASSA", "PUNTATA MEDIA", "PUNTATA ALTA" },              // it-IT
		{ "APUESTA BAJA", "APUESTA MEDIA", "APUESTA ALTA" },               // es-ES
		{ "APOSTA BAIXA", "APOSTA MÉDIA", "APOSTA ALTA" },                 // pt-BR
		{ "NISKI ZAKŁAD", "ŚREDNI ZAKŁAD", "WYSOKI ZAKŁAD" },              // pl-PL
		{ "НИЗКАЯ СТАВКА", "СРЕДНЯЯ СТАВКА", "ВЫСОКАЯ СТАВКА" },           // ru-RU
		{ "낮은 배팅", "중간 배팅", "높은 배팅" },                          // ko-KR
		{ "低注", "中注", "高注" },                                        // zh-TW
		{ "ベット低", "ベット中", "ベット高" },                            // ja-JP
		{ "APUESTA BAJA", "APUESTA MEDIA", "APUESTA ALTA" },               // es-MX
		{ "低注", "中注", "高注" },                                        // zh-CN
	};

	// "Insurance: YES"/"Insurance: No" -- see DrawInsuranceStatus().
	// Column 0 = YES, column 1 = No (matches the bool takeInsurance
	// caller convention below). Kept as one combined string per
	// language/state rather than a separate "Insurance:" prefix +
	// Yes/No pair, matching the original English's single-string
	// convention exactly. The "Insurance" noun itself is confirmed
	// against real sources for every language below (Assurance/
	// Versicherung/Assicurazione/Seguro/Ubezpieczenie/Страховка/보험/
	// 保險/保险/インシュアランス all matched real casino glossaries
	// exactly); only the YES/No half needed fixing (accents, and the
	// ja-JP transliteration).
	constexpr int kInsuranceLabelCount = 2;
	constexpr std::string_view kInsuranceLabels[kLanguageCount][kInsuranceLabelCount] =
	{
		{ "Insurance: YES", "Insurance: No" },                       // en-US
		{ "Assurance : OUI", "Assurance : Non" },                     // fr-FR
		{ "Versicherung: JA", "Versicherung: Nein" },                 // de-DE
		{ "Assicurazione: SÌ", "Assicurazione: No" },                 // it-IT -- SÌ needs the grave accent (bare "si" is the reflexive pronoun)
		{ "Seguro: SÍ", "Seguro: No" },                               // es-ES -- SÍ needs the accent (bare "si" is the conditional "if")
		{ "Seguro: SIM", "Seguro: Não" },                             // pt-BR
		{ "Ubezpieczenie: TAK", "Ubezpieczenie: Nie" },               // pl-PL
		{ "Страховка: ДА", "Страховка: Нет" },                        // ru-RU
		{ "보험: 예", "보험: 아니오" },                                // ko-KR
		{ "保險：是", "保險：否" },                                    // zh-TW
		{ "インシュアランス: はい", "インシュアランス: いいえ" },        // ja-JP -- confirmed transliteration is インシュアランス, not インシュランス (missing ア)
		{ "Seguro: SÍ", "Seguro: No" },                               // es-MX
		{ "保险：是", "保险：否" },                                    // zh-CN
	};

	// "Next cards:" -- label above the next-card-if-you-Hit icon strip,
	// see DrawNextCardStatus(). Not a real casino term (this mod's own
	// deck-prediction feature has no textbook equivalent), so only a
	// diacritics pass applies here -- restoring accents an earlier
	// ASCII-safety pass had stripped, now that /utf-8 is confirmed
	// working end-to-end.
	constexpr std::string_view kNextCardsLabel[kLanguageCount] =
	{
		"Next cards:",        // en-US
		"Prochaines cartes:", // fr-FR
		"Nächste Karten:",    // de-DE
		"Prossime carte:",    // it-IT
		"Próximas cartas:",   // es-ES
		"Próximas cartas:",   // pt-BR
		"Następne karty:",    // pl-PL
		"Следующие карты:",   // ru-RU
		"다음 카드:",           // ko-KR
		"下一張牌：",           // zh-TW
		"次のカード:",          // ja-JP
		"Próximas cartas:",   // es-MX
		"下一张牌：",           // zh-CN
	};

	Localization::Language g_current = Localization::Language::English;
	bool g_resolved = false; // true once Refresh() has actually run at least once

	Localization::Language ClampLanguage(std::int32_t raw)
	{
		if (raw < 0 || raw >= kLanguageCount)
			return Localization::Language::English;
		return static_cast<Localization::Language>(raw);
	}

	// BlackjackCheat.ini's [General] Language override -- "auto" (the
	// default) defers to the game's own current UI language; anything
	// else must match one of these exact codes, the same ones
	// LANGUAGE::_GET_CURRENT_LANGUAGE_ID()'s own return-value mapping
	// uses. Unrecognized text (a typo, or "auto" itself) falls back to
	// English via Refresh()'s caller.
	bool TryParseOverride(const std::string& code, Localization::Language& out)
	{
		if (code == "en-US") { out = Localization::Language::English; return true; }
		if (code == "fr-FR") { out = Localization::Language::French; return true; }
		if (code == "de-DE") { out = Localization::Language::German; return true; }
		if (code == "it-IT") { out = Localization::Language::Italian; return true; }
		if (code == "es-ES") { out = Localization::Language::Spanish; return true; }
		if (code == "pt-BR") { out = Localization::Language::PortugueseBrazilian; return true; }
		if (code == "pl-PL") { out = Localization::Language::Polish; return true; }
		if (code == "ru-RU") { out = Localization::Language::Russian; return true; }
		if (code == "ko-KR") { out = Localization::Language::Korean; return true; }
		if (code == "zh-TW") { out = Localization::Language::ChineseTraditional; return true; }
		if (code == "ja-JP") { out = Localization::Language::Japanese; return true; }
		if (code == "es-MX") { out = Localization::Language::SpanishMexican; return true; }
		if (code == "zh-CN") { out = Localization::Language::ChineseSimplified; return true; }
		return false;
	}
}

namespace Localization
{
	void Refresh()
	{
		const std::string& languageOverride = Config::Get().Language;

		Language resolved;
		if (TryParseOverride(languageOverride, resolved))
		{
			g_current = resolved;
		}
		else
		{
			// Covers "auto" (the documented default) and any typo'd
			// override alike -- both should fall back to the game's own
			// current language rather than silently forcing English.
			std::int32_t raw = LANGUAGE::_GET_CURRENT_LANGUAGE_ID();
			g_current = ClampLanguage(raw);
		}

		g_resolved = true;
		Log::Write("Localization::Refresh -> language index {} (ini override='{}')", static_cast<int>(g_current), languageOverride);
	}

	Language Current()
	{
		if (!g_resolved)
			Refresh();

		return g_current;
	}

	std::string_view ActionName(BlackjackHandEval::Action action)
	{
		int col = static_cast<int>(action);
		if (col < 0 || col >= kActionLabelCount)
			return "?";
		return kActionLabels[static_cast<int>(Current())][col];
	}

	std::string_view BettingConfidenceLabel(BlackjackHandEval::BettingConfidence confidence)
	{
		int col = static_cast<int>(confidence);
		if (col < 0 || col >= kBettingConfidenceLabelCount)
			return "?";
		return kBettingConfidenceLabels[static_cast<int>(Current())][col];
	}

	std::string_view InsuranceLabel(bool takeInsurance)
	{
		return kInsuranceLabels[static_cast<int>(Current())][takeInsurance ? 0 : 1];
	}

	std::string_view NextCardsLabel()
	{
		return kNextCardsLabel[static_cast<int>(Current())];
	}

	std::string_view LanguageCode(Language lang)
	{
		switch (lang)
		{
			case Language::English: return "en-US";
			case Language::French: return "fr-FR";
			case Language::German: return "de-DE";
			case Language::Italian: return "it-IT";
			case Language::Spanish: return "es-ES";
			case Language::PortugueseBrazilian: return "pt-BR";
			case Language::Polish: return "pl-PL";
			case Language::Russian: return "ru-RU";
			case Language::Korean: return "ko-KR";
			case Language::ChineseTraditional: return "zh-TW";
			case Language::Japanese: return "ja-JP";
			case Language::SpanishMexican: return "es-MX";
			case Language::ChineseSimplified: return "zh-CN";
			default: return "?";
		}
	}
}
