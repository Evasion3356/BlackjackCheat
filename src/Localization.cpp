#include "Localization.h"
#include "Config.h"
#include "Log.h"
#include "script.h" // LANGUAGE::_GET_CURRENT_LANGUAGE_ID() (natives.h, via script.h)

#include <string>

namespace
{
	constexpr int kLanguageCount = static_cast<int>(Localization::Language::Count);

	// HIT/STAND/DOUBLE/SPLIT -- the standalone advice readout (see
	// DrawAdviceStatus() in BlackjackCheat.cpp). Kept as short, punchy
	// all-caps imperatives in every language, matching the English
	// original's tone (a HUD command, not a sentence). One row per
	// Language (see Localization.h's enum, same ordering), 4 columns
	// matching BlackjackHandEval::Action's declaration order (Hit,
	// Stand, Double, Split). Translations beyond row 0 (English) are
	// LLM-assisted, not yet reviewed by a native speaker per language --
	// fix a row directly here if a wording turns out to be wrong.
	constexpr int kActionLabelCount = 4;
	const char* const kActionLabels[kLanguageCount][kActionLabelCount] =
	{
		{ "HIT", "STAND", "DOUBLE", "SPLIT" },                     // en-US
		{ "TIRER", "RESTER", "DOUBLER", "SEPARER" },                // fr-FR
		{ "ZIEHEN", "HALTEN", "VERDOPPELN", "TEILEN" },             // de-DE
		{ "CARTA", "STARE", "RADDOPPIA", "DIVIDI" },                // it-IT
		{ "PEDIR", "PLANTARSE", "DOBLAR", "DIVIDIR" },              // es-ES
		{ "PEDIR CARTA", "PARAR", "DOBRAR", "DIVIDIR" },            // pt-BR
		{ "DOBIERZ", "STÓJ", "PODWÓJ", "PODZIEL" },                 // pl-PL
		{ "ЕЩЕ", "СТОП", "УДВОИТЬ", "РАЗДЕЛИТЬ" },                  // ru-RU
		{ "히트", "스탠드", "더블", "스플릿" },                        // ko-KR
		{ "要牌", "停牌", "加倍", "分牌" },                          // zh-TW
		{ "ヒット", "スタンド", "ダブル", "スプリット" },              // ja-JP
		{ "PEDIR", "PLANTARSE", "DOBLAR", "DIVIDIR" },              // es-MX -- same as es-ES, no meaningful regional difference for these terms
		{ "要牌", "停牌", "加倍", "分牌" },                          // zh-CN
	};

	// BET LOW/MEDIUM/HIGH -- the betting-advice readout (see
	// DrawBettingAdviceStatus()). Column order matches
	// BlackjackHandEval::BettingConfidence's declaration order (Low,
	// Medium, High).
	constexpr int kBettingConfidenceLabelCount = 3;
	const char* const kBettingConfidenceLabels[kLanguageCount][kBettingConfidenceLabelCount] =
	{
		{ "BET LOW", "BET MEDIUM", "BET HIGH" },                          // en-US
		{ "MISE FAIBLE", "MISE MOYENNE", "MISE ELEVEE" },                  // fr-FR
		{ "NIEDRIG SETZEN", "MITTEL SETZEN", "HOCH SETZEN" },              // de-DE
		{ "PUNTATA BASSA", "PUNTATA MEDIA", "PUNTATA ALTA" },              // it-IT
		{ "APUESTA BAJA", "APUESTA MEDIA", "APUESTA ALTA" },               // es-ES
		{ "APOSTA BAIXA", "APOSTA MEDIA", "APOSTA ALTA" },                 // pt-BR
		{ "NISKI ZAKLAD", "SREDNI ZAKLAD", "WYSOKI ZAKLAD" },              // pl-PL
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
	// convention exactly.
	constexpr int kInsuranceLabelCount = 2;
	const char* const kInsuranceLabels[kLanguageCount][kInsuranceLabelCount] =
	{
		{ "Insurance: YES", "Insurance: No" },                       // en-US
		{ "Assurance : OUI", "Assurance : Non" },                     // fr-FR
		{ "Versicherung: JA", "Versicherung: Nein" },                 // de-DE
		{ "Assicurazione: SI", "Assicurazione: No" },                 // it-IT
		{ "Seguro: SI", "Seguro: No" },                               // es-ES
		{ "Seguro: SIM", "Seguro: Nao" },                             // pt-BR
		{ "Ubezpieczenie: TAK", "Ubezpieczenie: Nie" },               // pl-PL
		{ "Страховка: ДА", "Страховка: Нет" },                        // ru-RU
		{ "보험: 예", "보험: 아니오" },                                // ko-KR
		{ "保險：是", "保險：否" },                                    // zh-TW
		{ "インシュランス: はい", "インシュランス: いいえ" },            // ja-JP
		{ "Seguro: SI", "Seguro: No" },                               // es-MX
		{ "保险：是", "保险：否" },                                    // zh-CN
	};

	// "Next cards:" -- label above the next-card-if-you-Hit icon strip,
	// see DrawNextCardStatus().
	const char* const kNextCardsLabel[kLanguageCount] =
	{
		"Next cards:",       // en-US
		"Prochaines cartes:", // fr-FR
		"Naechste Karten:",   // de-DE
		"Prossime carte:",    // it-IT
		"Proximas cartas:",   // es-ES
		"Proximas cartas:",   // pt-BR
		"Nastepne karty:",    // pl-PL
		"Следующие карты:",   // ru-RU
		"다음 카드:",           // ko-KR
		"下一張牌：",           // zh-TW
		"次のカード:",          // ja-JP
		"Proximas cartas:",   // es-MX
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

	const char* ActionName(BlackjackHandEval::Action action)
	{
		int col = static_cast<int>(action);
		if (col < 0 || col >= kActionLabelCount)
			return "?";
		return kActionLabels[static_cast<int>(Current())][col];
	}

	const char* BettingConfidenceLabel(BlackjackHandEval::BettingConfidence confidence)
	{
		int col = static_cast<int>(confidence);
		if (col < 0 || col >= kBettingConfidenceLabelCount)
			return "?";
		return kBettingConfidenceLabels[static_cast<int>(Current())][col];
	}

	const char* InsuranceLabel(bool takeInsurance)
	{
		return kInsuranceLabels[static_cast<int>(Current())][takeInsurance ? 0 : 1];
	}

	const char* NextCardsLabel()
	{
		return kNextCardsLabel[static_cast<int>(Current())];
	}

	const char* LanguageCode(Language lang)
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
