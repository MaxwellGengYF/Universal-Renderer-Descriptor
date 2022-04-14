

#include <Graphics/ShaderCompiler/CodeSample/ISyntaxCompare.h>
void ISyntaxCompare::AddSubSyntax(ISyntaxCompare* syn)
{
	subSyntax.push_back(syn);
}
void ISyntaxCompare::ClearSynbSyntex()
{
	subSyntax.clear();
}
ISyntaxCompare::ISyntaxCompare(bool neccessary) : neccessary(neccessary)
{

}

bool ISyntaxCompare::SelfSyntaxMatch(
	ISyntaxCompare* parent,
	char const*& currentPtr,
	char const* endPtr
)
{
	Step step;
	do
	{
		if (currentPtr >= endPtr)
			return !neccessary;
		step = Match(parent, currentPtr, endPtr);
	} while (step == Step::Continue);

	if (step == Step::Avaliable)
	{
		captured = true;
		for (auto subIte = subSyntax.begin(); subIte != subSyntax.end(); ++subIte)
		{
			if (!(*subIte)->SelfSyntaxMatch(
				this, currentPtr, endPtr
			)) return false;
		}
		return true;
	}
	captured = false;
	return !neccessary;
}
void ISyntaxCompare::SyntaxClear()
{
	captured = false;
	Clear();
	for (auto subIte = subSyntax.begin(); subIte != subSyntax.end(); ++subIte)
	{
		(*subIte)->Clear();
		(*subIte)->captured = false;
	}
}
 ISyntaxCompare::~ISyntaxCompare() {}