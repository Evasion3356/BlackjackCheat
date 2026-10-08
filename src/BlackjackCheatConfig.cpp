#include "Config.h"

// The library's side of Config: the live values and their option table. The
// INI loading (Reload) is the ASI's, in Config.cpp.
namespace BlackjackCheat::Config
{
	Values& Mutable()
	{
		static Values values;
		return values;
	}

	const Values& Get()
	{
		return Mutable();
	}

	std::span<const Option> Options()
	{
		using enum Option::Kind;
		Values& v = Mutable();
		static const Option options[] = {
			{ "showdeckprediction", "HUD", "Show Deck Prediction", "Shows the dealer's real hole card and the next cards off the deck.", Bool, &v.ShowDeckPrediction },
			{ "showcardsbeforebet", "HUD", "Show Cards Before Bet", "Before the deal, shows the hands the deck will give you and the dealer.", Bool, &v.ShowCardsBeforeBet },
			{ "showdealerhand", "HUD", "Show Dealer Hand", "Shows the dealer's cards (the real hole card and the predicted pair).", Bool, &v.ShowDealerHand },
			{ "showadvice", "HUD", "Show Advice", "Hit, stand, double, split and insurance advice from the deck ahead.", Bool, &v.ShowAdvice },
			{ "showbettingadvice", "HUD", "Show Betting Advice", "Before the deal, whether to bet max or min, from the deck ahead.", Bool, &v.ShowBettingAdvice },
			{ "bethotkeys", "Controls", "Bet Hotkeys", "Right/Left arrow: bet 5 steps more or less. Tab: bet max.", Bool, &v.BetHotkeys },
#ifdef _DEBUG
			{ "panelx", "HUD Layout", "Panel X", "Text panel position.", Float, &v.PanelX, 0.0f, 1.0f, 0.005f },
			{ "panely", "HUD Layout", "Panel Y", "Text panel position.", Float, &v.PanelY, 0.0f, 1.0f, 0.005f },
			{ "textscale", "HUD Layout", "Text Scale", "Text panel scale.", Float, &v.TextScale, 0.1f, 1.0f, 0.01f },
			{ "titletextscale", "HUD Layout", "Title Text Scale", "Text panel title scale.", Float, &v.TitleTextScale, 0.1f, 1.0f, 0.01f },
			{ "advicex", "HUD Layout", "Advice X", "Advice readout position.", Float, &v.AdviceX, 0.0f, 1.0f, 0.005f },
			{ "advicey", "HUD Layout", "Advice Y", "Advice readout position.", Float, &v.AdviceY, 0.0f, 1.0f, 0.005f },
			{ "holecardiconx", "HUD Layout", "Hole Card X", "Dealer hole card icon position.", Float, &v.HoleCardIconX, 0.0f, 1.0f, 0.001f },
			{ "holecardicony", "HUD Layout", "Hole Card Y", "Dealer hole card icon position.", Float, &v.HoleCardIconY, 0.0f, 1.0f, 0.001f },
			{ "holecardiconwidth", "HUD Layout", "Hole Card Width", "Dealer hole card icon size.", Float, &v.HoleCardIconWidth, 0.0f, 0.2f, 0.001f },
			{ "holecardiconheight", "HUD Layout", "Hole Card Height", "Dealer hole card icon size.", Float, &v.HoleCardIconHeight, 0.0f, 0.2f, 0.001f },
			{ "nextcardiconbasex", "HUD Layout", "Next Cards X", "Next cards strip position.", Float, &v.NextCardIconBaseX, 0.0f, 1.0f, 0.001f },
			{ "nextcardicony", "HUD Layout", "Next Cards Y", "Next cards strip position.", Float, &v.NextCardIconY, 0.0f, 1.0f, 0.001f },
			{ "nextcardiconspacingx", "HUD Layout", "Next Cards Spacing", "Next cards spacing.", Float, &v.NextCardIconSpacingX, 0.0f, 0.2f, 0.001f },
			{ "nextcardiconwidth", "HUD Layout", "Next Card Width", "Next card icon size.", Float, &v.NextCardIconWidth, 0.0f, 0.2f, 0.001f },
			{ "nextcardiconheight", "HUD Layout", "Next Card Height", "Next card icon size.", Float, &v.NextCardIconHeight, 0.0f, 0.2f, 0.001f },
			{ "myhandiconx", "HUD Layout", "My Hand X", "Your hand icons position.", Float, &v.MyHandIconX, 0.0f, 1.0f, 0.001f },
			{ "myhandicony", "HUD Layout", "My Hand Y", "Your hand icons position.", Float, &v.MyHandIconY, 0.0f, 1.0f, 0.001f },
			{ "myhandiconspacingx", "HUD Layout", "My Hand Spacing", "Your hand icons spacing.", Float, &v.MyHandIconSpacingX, 0.0f, 0.2f, 0.001f },
			{ "myhandiconwidth", "HUD Layout", "My Hand Icon Width", "Your hand icon size.", Float, &v.MyHandIconWidth, 0.0f, 0.2f, 0.001f },
			{ "myhandiconheight", "HUD Layout", "My Hand Icon Height", "Your hand icon size.", Float, &v.MyHandIconHeight, 0.0f, 0.2f, 0.001f },
#endif
		};
		return options;
	}
}
