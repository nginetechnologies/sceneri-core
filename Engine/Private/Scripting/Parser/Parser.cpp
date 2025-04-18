#include "Engine/Scripting/Parser/Parser.h"
#include "Engine/Scripting/Parser/Token.h"
#include "Engine/Scripting/Parser/TokenTypeDefinition.h"
#include "Engine/Scripting/Parser/TokenTypeLiterals.h"
#include "Engine/Scripting/Parser/ScriptValue.h"

#include "Engine/Scripting/Parser/AST/Statement.h"
#include "Engine/Entity/Component2D.h"
#include "Engine/Entity/Component3D.h"
#include "Engine/Entity/Manager.h"
#include "Engine/Asset/Identifier.h"
#include "Engine/Tag/TagIdentifier.h"

#include <Common/Scripting/VirtualMachine/DynamicFunction/DynamicFunction.h>
#include <Common/Memory/Variant.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Reflection/GenericType.h>
#include <Common/Reflection/Registry.inl>
#include <Common/IO/Format/Path.h>
#include <Common/IO/Log.h>
#include <Common/Math/Vector2.h>
#include <Common/Math/Vector3.h>
#include <Common/Math/WorldCoordinate.h>
#include <Common/Math/Vector4.h>
#include <Common/Math/Color.h>
#include <Common/Math/Rotation2D.h>
#include <Common/Math/Quaternion.h>
#include <Common/Math/Transform2D.h>
#include <Common/Math/Transform.h>
#include <Common/Math/Angle.h>
#include <Common/Math/Density.h>
#include <Common/Math/Length.h>
#include <Common/Math/Mass.h>
#include <Common/Math/Ratio.h>
#include <Common/Math/RotationalSpeed.h>
#include <Common/Math/Speed.h>
#include <Common/Math/Torque.h>

namespace ngine::Scripting
{
	// Specifies the precedence for the given prefix token type
	// 11 = 'not', '-', '~', '#'
	[[nodiscard]] constexpr int32 GetPrefixPrecedence(const TokenType type)
	{
		switch (type)
		{
			case TokenType::Minus:
			case TokenType::Tilde:
			case TokenType::Hashtag:
			case TokenType::Not:
				return 11;
			default:
				return 0;
		}
	}

	// Specifies the precedence for the given infix token type
	//  1 = 'or'
	//  2 = 'and'
	//  3 = '<', '>', '<=', '>=', '~=', '=='
	//  4 = '|'
	//  5 = '~'
	//  6 = '&'
	//  7 = '<<', '>>'
	//  8 = '..'
	//  9 = '+', '-'
	// 10 = '*', '/', '//', '%'
	// (11 = 'not', '-', '~', '#') prefix, see above
	// 12 = '^'
	[[nodiscard]] constexpr int32 GetInfixPrecedence(const TokenType type)
	{
		switch (type)
		{
			case TokenType::Or:
				return 1;
			case TokenType::And:
				return 2;
			case TokenType::Less:
			case TokenType::Greater:
			case TokenType::LessEqual:
			case TokenType::GreaterEqual:
			case TokenType::NotEqual:
			case TokenType::EqualEqual:
				return 3;
			case TokenType::Pipe:
				return 4;
			case TokenType::Not:
				return 5;
			case TokenType::Ampersand:
				return 6;
			case TokenType::LessLess:
			case TokenType::GreaterGreater:
				return 7;
			case TokenType::DotDot:
				return 8;
			case TokenType::Plus:
			case TokenType::Minus:
				return 9;
			case TokenType::Star:
			case TokenType::Slash:
			case TokenType::SlashSlash:
			case TokenType::Percent:
				return 10;
			case TokenType::Exponent:
				return 12;
			default:
				return 0;
		}
	}

	Optional<AST::Graph> Parser::Parse(const TokenListType& tokens)
	{
		Assert(!m_graph.IsValid());
		m_state = State{};
		m_state.begin = m_state.current = tokens.begin();
		m_state.end = tokens.end() - 1;
		Assert(m_state.end->type == TokenType::Eof);

		StatementChunk();

		return m_state.flags.IsNotSet(Flags::Error) ? Move(m_graph) : Optional<AST::Graph>{};
	}

	// chunk ::= block
	Optional<AST::Statement::Block*> Parser::StatementChunk()
	{
		return StatementBlock();
	}

	// block ::= {stat} [retstat]
	Optional<AST::Statement::Block*> Parser::StatementBlock()
	{
		AST::Statements stmtlist;
		ParseBlock(stmtlist);

		return m_graph.EmplaceNode<AST::Statement::Block>(Move(stmtlist));
	}

	// retstat ::= return [explist] [‘;’]
	Optional<AST::Statement::Return*> Parser::StatementReturn()
	{
		const Token& returnKeyword = Previous();

		AST::Expressions exprlist;
		ParseExprlist(exprlist);

		Match(TokenType::Semicolon);

		return m_graph.EmplaceNode<AST::Statement::Return>(Move(exprlist), returnKeyword.sourceLocation);
	}

	bool DoReturnTypesMatchSignature(
		const ArrayView<const Scripting::Type> functionReturnTypes, const ArrayView<const VariableToken> expectedReturnTypes
	)
	{
		if (functionReturnTypes.GetSize() != expectedReturnTypes.GetSize())
		{
			return false;
		}

		for (uint32 i = 0, count = functionReturnTypes.GetSize(); i < count; ++i)
		{
			const Scripting::Type& functionReturnType = functionReturnTypes[i];
			const VariableToken& expectedReturnTypeToken = expectedReturnTypes[i];
			Assert(expectedReturnTypeToken.m_types.GetSize() == 1);
			if (functionReturnType.GetType() != expectedReturnTypeToken.m_types[0].GetType())
			{
				return false;
			}

			if (functionReturnType != expectedReturnTypeToken.m_types[0])
			{
				return false;
			}
		}

		return true;
	}

	[[nodiscard]] Reflection::DynamicTypeDefinition CreateVariant(const ArrayView<const Scripting::Type> types)
	{
		Vector<Reflection::TypeDefinition> typeDefinitions;
		typeDefinitions.Reserve(types.GetSize());
		for (const Scripting::Type& type : types)
		{
			typeDefinitions.EmplaceBack(type);
		}
		return Reflection::DynamicTypeDefinition::MakeVariant(typeDefinitions);
	}

	[[nodiscard]] Types CreateReturnTypes(const ArrayView<const VariableToken> variableTokens)
	{
		if (variableTokens.GetSize() == 0)
		{
			return {};
		}
		else if (variableTokens.GetSize() == 1)
		{
			return variableTokens[0].m_types.GetView();
		}
		else
		{
			Types returnTypes;
			returnTypes.Reserve(variableTokens.GetSize());
			for (const VariableToken& returnTypeToken : variableTokens)
			{
				Assert(returnTypeToken.m_types.HasElements());
				if (returnTypeToken.m_types.GetSize() == 1)
				{
					returnTypes.EmplaceBack(returnTypeToken.m_types[0]);
				}
				else
				{
					returnTypes.EmplaceBack(CreateVariant(returnTypeToken.m_types));
				}
			}
			return Move(returnTypes);
		}
	}

	bool Type::Serialize(const Serialization::Reader reader)
	{
		if (reader.IsObject())
		{
			const Reflection::TypeDefinitionType type =
				reader.ReadWithDefaultValue<Reflection::TypeDefinitionType>("type", Reflection::TypeDefinitionType::Native);
			switch (type)
			{
				case Reflection::TypeDefinitionType::Invalid:
					return false;
				case Reflection::TypeDefinitionType::Native:
				{
					if (Optional<Reflection::TypeDefinition> typeDefinition = reader.ReadInPlace<Reflection::TypeDefinition>())
					{
						*this = Move(*typeDefinition);
						return true;
					}
					else
					{
						return false;
					}
				}
				case Reflection::TypeDefinitionType::Structure:
				case Reflection::TypeDefinitionType::Variant:
				{
					if (Optional<Reflection::DynamicTypeDefinition> typeDefinition = reader.ReadInPlace<Reflection::DynamicTypeDefinition>())
					{
						*this = Move(*typeDefinition);
						return true;
					}
					else
					{
						return false;
					}
				}
			}
			ExpectUnreachable();
		}
		else if (reader.IsString())
		{
			if (Optional<Reflection::TypeDefinition> typeDefinition = reader.ReadInPlace<Reflection::TypeDefinition>())
			{
				*this = Move(*typeDefinition);
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	bool Type::Serialize(Serialization::Writer writer) const
	{
		return BaseType::Visit(
			[writer](const Reflection::TypeDefinition typeDefinition) mutable -> bool
			{
				return writer.SerializeInPlace(typeDefinition);
			},
			[writer](const Reflection::DynamicTypeDefinition& typeDefinition) mutable -> bool
			{
				return writer.SerializeInPlace(typeDefinition);
			},
			[]()
			{
				return false;
			}
		);
	}

	// stat :: = ‘;’ |
	//			 varlist ‘=’ explist |
	//			 functioncall |
	//			 label |
	//			 break |
	//			 goto Name |
	//			 do block end |
	//			 while exp do block end |
	//			 repeat block until exp |
	//			 if exp then block {elseif exp then block} [else block] end |
	//			 for Name ‘=’ exp ‘,’ exp [‘,’ exp] do block end |
	//			 for namelist in explist do block end |
	//			 function funcname funcbody |
	//			 local function Name funcbody |
	//			 local attnamelist [‘=’ explist]
	Optional<AST::Statement::Base*> Parser::Statement()
	{
		if (Match(TokenType::Semicolon))
		{
			return nullptr;
		}
		// TODO(Ben): Implement label
		if (Match(TokenType::Break))
		{
			return StatementBreak();
		}
		// TODO(Ben): Implement goto
		if (Match(TokenType::Do))
		{
			return StatementDo();
		}
		if (Match(TokenType::While))
		{
			return StatementWhile();
		}
		if (Match(TokenType::Repeat))
		{
			return StatementRepeat();
		}
		if (Match(TokenType::If))
		{
			return StatementIf();
		}
		// TODO(Ben): Implement for
		if (Match(TokenType::Function))
		{
			// funcname ::= Name {‘.’ Name} [‘:’ Name]
			if (UNLIKELY_ERROR(!Match(TokenType::Identifier)))
			{
				Error(SCRIPT_STRING_LITERAL("Expected function name after 'function'"));
				return nullptr;
			}
			const Token& identifier = Previous();
			if (Check(TokenType::Period))
			{
				// TODO(Ben): Implement {‘.’ Name}
			}
			if (Check(TokenType::Colon))
			{
				// TODO(Ben): Implement [‘:’ Name]
			}

			const bool wasDeclared =
				EmplaceGlobalVariableType(identifier.identifier, Reflection::TypeDefinition::Get<VM::DynamicFunction>()).wasDeclared;
			Assert(wasDeclared);
			if (!wasDeclared)
			{
				Error("Function cannot be redeclared");
			}

			State previousState = Move(m_state);
			m_state.pEnclosingState = previousState;
			AST::Expression::Function functionBody = FunctionBody();
			TokenListType::const_iterator current = m_state.current;
			m_state = Move(previousState);
			m_state.current = current;

			AST::Expression::Function& function = m_graph.EmplaceNode<AST::Expression::Function>(Move(functionBody));

			Assert(!m_state.m_functionReturnTypes.Contains(identifier.identifier));
			m_state.m_functionReturnTypes.Emplace(Guid{identifier.identifier}, CreateReturnTypes(function.GetReturnTypes()));

			AST::Expressions initializers;
			initializers.EmplaceBack(function);

			AST::Expressions varlist;
			const bool isLocal = false;
			varlist.EmplaceBack(m_graph.EmplaceNode<AST::Expression::VariableDeclaration>(
				VariableToken{Token{identifier}, Reflection::TypeDefinition::Get<VM::DynamicFunction>()},
				isLocal
			));

			AST::Expression::Base& assignment = m_graph.EmplaceNode<AST::Expression::Assignment>(Move(varlist), Move(initializers));
			return m_graph.EmplaceNode<AST::Statement::Expression>(assignment);
		}
		if (Match(TokenType::Local))
		{
			if (Match(TokenType::Function))
			{
				if (UNLIKELY_ERROR(!Match(TokenType::Identifier)))
				{
					Error(SCRIPT_STRING_LITERAL("Expected function name after 'local function'"));
					return nullptr;
				}

				const Token& identifier = Previous();

				State previousState = Move(m_state);
				m_state.pEnclosingState = previousState;
				AST::Expression::Function functionBody = FunctionBody();
				TokenListType::const_iterator current = m_state.current;
				m_state = Move(previousState);
				m_state.current = current;

				if (auto functionIt = m_state.m_functionReturnTypes.Find(identifier.identifier); functionIt != m_state.m_functionReturnTypes.end())
				{
					functionIt->second = CreateReturnTypes(functionBody.GetReturnTypes());
				}
				else
				{
					m_state.m_functionReturnTypes.Emplace(Guid{identifier.identifier}, CreateReturnTypes(functionBody.GetReturnTypes()));
				}

				AST::Expressions initializers;
				initializers.EmplaceBack(m_graph.EmplaceNode<AST::Expression::Function>(Move(functionBody)));

				AST::Expressions varlist;
				const bool isLocal = true;
				DeclareLocalVariableType(identifier.identifier, Reflection::TypeDefinition::Get<VM::DynamicFunction>());
				varlist.EmplaceBack(m_graph.EmplaceNode<AST::Expression::VariableDeclaration>(
					VariableToken{Token{identifier}, Reflection::TypeDefinition::Get<VM::DynamicFunction>()},
					isLocal
				));

				AST::Expression::Base& assignment = m_graph.EmplaceNode<AST::Expression::Assignment>(Move(varlist), Move(initializers));
				return m_graph.EmplaceNode<AST::Statement::Expression>(assignment);
			}
			else
			{
				AST::Expression::Variable::Tokens names;
				ParseNamelist(names);
				if (UNLIKELY_ERROR(names.IsEmpty()))
				{
					Error(SCRIPT_STRING_LITERAL("Expected at least one variable name after 'local'"));
					return nullptr;
				}

				AST::Expressions varlist;
				const bool isLocal = true;
				for (const VariableToken& name : names)
				{
					DeclareLocalVariableType(name.identifier, name.m_types.GetView());
					varlist.EmplaceBack(m_graph.EmplaceNode<AST::Expression::VariableDeclaration>(VariableToken{name}, isLocal));
				}

				AST::Expressions exprlist;
				if (Match(TokenType::Equal))
				{
					ParseExprlist(exprlist, varlist.GetView());

					// Resolve "any" / "auto" variables based on usage
					for (const ReferenceWrapper<AST::Expression::Base>& variable : varlist)
					{
						switch (variable->GetType())
						{
							case AST::NodeType::VariableDeclaration:
							{
								AST::Expression::VariableDeclaration& variableDeclaration = static_cast<AST::Expression::VariableDeclaration&>(*variable);
								const uint32 variableIndex = varlist.GetView().GetIteratorIndex(Memory::GetAddressOf(variable));
								if (variableIndex < exprlist.GetSize())
								{
									if (variableDeclaration.GetIdentifier().m_types[0].Is<nullptr_type>())
									{
										Types allowedTypes = GetPossibleExpressionReturnTypes(exprlist.GetView()[variableIndex]);
										variableDeclaration.SetAllowedTypes(allowedTypes.GetView());

										if (variableDeclaration.IsLocal())
										{
											UpdateLocalVariableType(variableDeclaration.GetIdentifier().identifier, Move(allowedTypes));
										}
										else
										{
											UpdateGlobalVariableType(variableDeclaration.GetIdentifier().identifier, Move(allowedTypes));
										}
									}
								}
								else if (variableDeclaration.GetIdentifier().m_types[0].Is<nullptr_type>())
								{
									Scripting::Type allowedType{Reflection::TypeDefinition::Get<nullptr_type>()};
									variableDeclaration.SetAllowedTypes(Scripting::Type{allowedType});

									if (variableDeclaration.IsLocal())
									{
										UpdateLocalVariableType(variableDeclaration.GetIdentifier().identifier, Move(allowedType));
									}
									else
									{
										UpdateGlobalVariableType(variableDeclaration.GetIdentifier().identifier, Move(allowedType));
									}
								}
							}
							break;
							case AST::NodeType::Variable:
							{
								AST::Expression::Variable& variableExpression = static_cast<AST::Expression::Variable&>(*variable);
								const uint32 variableIndex = varlist.GetView().GetIteratorIndex(Memory::GetAddressOf(variable));
								if (variableIndex < exprlist.GetSize())
								{
									if (variableExpression.GetIdentifier().m_types[0].Is<nullptr_type>())
									{
										Types allowedTypes = GetPossibleExpressionReturnTypes(exprlist.GetView()[variableIndex]);
										variableExpression.SetAllowedTypes(allowedTypes.GetView());

										UpdateLocalVariableType(variableExpression.GetIdentifier().identifier, Move(allowedTypes));
									}
								}
								else if (variableExpression.GetIdentifier().m_types[0].Is<nullptr_type>())
								{
									Scripting::Type allowedType{Reflection::TypeDefinition::Get<nullptr_type>()};
									variableExpression.SetAllowedTypes(Scripting::Type{allowedType});

									UpdateVariableType(variableExpression.GetIdentifier().identifier, Move(allowedType));
								}
							}
							break;
							default:
								ExpectUnreachable();
						}
					}
				}
				AST::Expression::Base& assignment = m_graph.EmplaceNode<AST::Expression::Assignment>(Move(varlist), Move(exprlist));
				return m_graph.EmplaceNode<AST::Statement::Expression>(assignment);
			}
		}

		if (Optional<AST::Expression::Base*> pExpression = Prefix())
		{
			if (pExpression->GetType() == AST::NodeType::VariableDeclaration || pExpression->GetType() == AST::NodeType::Variable)
			{
				// varlist
				AST::Expressions varlist;
				varlist.EmplaceBack(*pExpression);
				while (Match(TokenType::Comma))
				{
					Optional<AST::Expression::Base*> pVariable = Prefix();
					varlist.EmplaceBack(*pVariable);
				}

				// Allow := syntax
				Match(TokenType::Semicolon);
				if (UNLIKELY_ERROR(!Match(TokenType::Equal)))
				{
					Error(SCRIPT_STRING_LITERAL("Expected '=' after varlist"));
					return nullptr;
				}

				// exprlist
				AST::Expressions exprlist;
				ParseExprlist(exprlist, varlist.GetView());

				// Resolve "any" / "auto" variables based on usage
				for (const ReferenceWrapper<AST::Expression::Base>& variable : varlist)
				{
					switch (variable->GetType())
					{
						case AST::NodeType::VariableDeclaration:
						{
							AST::Expression::VariableDeclaration& variableDeclaration = static_cast<AST::Expression::VariableDeclaration&>(*variable);
							const uint32 variableIndex = varlist.GetView().GetIteratorIndex(Memory::GetAddressOf(variable));
							if (variableIndex < exprlist.GetSize())
							{
								const AST::Expression::Base& expression = exprlist.GetView()[variableIndex];
								if (variableDeclaration.GetIdentifier().m_types[0].Is<nullptr_type>())
								{
									Types allowedTypes = GetPossibleExpressionReturnTypes(expression);
									variableDeclaration.SetAllowedTypes(allowedTypes.GetView());

									if (variableDeclaration.IsLocal())
									{
										UpdateLocalVariableType(variableDeclaration.GetIdentifier().identifier, Move(allowedTypes));
									}
									else
									{
										UpdateGlobalVariableType(variableDeclaration.GetIdentifier().identifier, Move(allowedTypes));
									}
								}

								if (expression.GetType() == AST::NodeType::Function)
								{
									const AST::Expression::Function& functionExpression = static_cast<const AST::Expression::Function&>(expression);
									if (m_state.m_functionReturnTypes.Contains(variableDeclaration.GetIdentifier().identifier))
									{
										const Types& functionReturnTypes =
											m_state.m_functionReturnTypes.Find(variableDeclaration.GetIdentifier().identifier)->second;
										if (DoReturnTypesMatchSignature(functionReturnTypes, functionExpression.GetReturnTypes()))
										{
											Error("Return type mismatch");
											return {};
										}
									}
									else
									{
										m_state.m_functionReturnTypes.Emplace(
											Guid{variableDeclaration.GetIdentifier().identifier},
											CreateReturnTypes(functionExpression.GetReturnTypes())
										);
									}
								}
							}
							else if (variableDeclaration.GetIdentifier().m_types[0].Is<nullptr_type>())
							{
								Scripting::Type allowedType{Reflection::TypeDefinition::Get<nullptr_type>()};
								variableDeclaration.SetAllowedTypes(Scripting::Type{allowedType});

								if (variableDeclaration.IsLocal())
								{
									UpdateLocalVariableType(variableDeclaration.GetIdentifier().identifier, Move(allowedType));
								}
								else
								{
									UpdateGlobalVariableType(variableDeclaration.GetIdentifier().identifier, Move(allowedType));
								}
							}
						}
						break;
						case AST::NodeType::Variable:
						{
							AST::Expression::Variable& variableExpression = static_cast<AST::Expression::Variable&>(*variable);
							const uint32 variableIndex = varlist.GetView().GetIteratorIndex(Memory::GetAddressOf(variable));
							if (variableIndex < exprlist.GetSize())
							{
								const AST::Expression::Base& expression = exprlist.GetView()[variableIndex];

								if (variableExpression.GetIdentifier().m_types[0].Is<nullptr_type>())
								{
									Types allowedTypes = GetPossibleExpressionReturnTypes(exprlist.GetView()[variableIndex]);
									variableExpression.SetAllowedTypes(allowedTypes.GetView());

									UpdateVariableType(variableExpression.GetIdentifier().identifier, Move(allowedTypes));
								}

								if (expression.GetType() == AST::NodeType::Function)
								{
									const AST::Expression::Function& functionExpression = static_cast<const AST::Expression::Function&>(expression);
									if (m_state.m_functionReturnTypes.Contains(variableExpression.GetIdentifier().identifier))
									{
										const Types& functionReturnTypes =
											m_state.m_functionReturnTypes.Find(variableExpression.GetIdentifier().identifier)->second;
										if (DoReturnTypesMatchSignature(functionReturnTypes, functionExpression.GetReturnTypes()))
										{
											Error("Return type mismatch");
											return {};
										}
									}
									else
									{
										m_state.m_functionReturnTypes
											.Emplace(Guid{variableExpression.GetIdentifier().identifier}, CreateReturnTypes(functionExpression.GetReturnTypes()));
									}
								}
							}
							else if (variableExpression.GetIdentifier().m_types[0].Is<nullptr_type>())
							{
								Scripting::Type allowedType{Reflection::TypeDefinition::Get<nullptr_type>()};
								variableExpression.SetAllowedTypes(Scripting::Type{allowedType});

								UpdateVariableType(variableExpression.GetIdentifier().identifier, Move(allowedType));
							}
						}
						break;
						default:
							ExpectUnreachable();
					}
				}

				AST::Expression::Base& assignment = m_graph.EmplaceNode<AST::Expression::Assignment>(Move(varlist), Move(exprlist));
				return m_graph.EmplaceNode<AST::Statement::Expression>(assignment);
			}
			else if (pExpression->GetType() == AST::NodeType::Call)
			{
				return m_graph.EmplaceNode<AST::Statement::Expression>(pExpression);
			}
		}

		Error(SCRIPT_STRING_LITERAL("Expected a valid statement"));
		return nullptr;
	}

	// break
	Optional<AST::Statement::Break*> Parser::StatementBreak()
	{
		if (UNLIKELY_ERROR(m_state.loopDepth == 0))
		{
			Error(SCRIPT_STRING_LITERAL("Expected 'break' inside a loop"));
			return nullptr;
		}
		return m_graph.EmplaceNode<AST::Statement::Break>();
	}

	// do block end
	Optional<AST::Statement::Base*> Parser::StatementDo()
	{
		State previousState = Move(m_state);
		m_state.pEnclosingState = previousState;

		Optional<AST::Statement::Base*> pStatement = StatementBlock();
		if (UNLIKELY_ERROR(!Match(TokenType::End)))
		{
			Error(SCRIPT_STRING_LITERAL("Expected 'end' after 'do' block"));
			return nullptr;
		}

		TokenListType::const_iterator current = m_state.current;
		m_state = Move(previousState);
		m_state.current = current;
		return pStatement;
	}

	// while exp do block end
	Optional<AST::Statement::While*> Parser::StatementWhile()
	{
		++m_state.loopDepth;
		Optional<AST::Expression::Base*> pCondition = Expression();
		if (UNLIKELY_ERROR(!Match(TokenType::Do)))
		{
			Error(SCRIPT_STRING_LITERAL("Expected 'do' after 'while'"));
			return nullptr;
		}

		State previousState = Move(m_state);
		m_state.pEnclosingState = previousState;
		Optional<AST::Statement::Base*> pBody = StatementBlock();
		TokenListType::const_iterator current = m_state.current;
		m_state = Move(previousState);
		m_state.current = current;

		--m_state.loopDepth;
		if (UNLIKELY_ERROR(!Match(TokenType::End)))
		{
			Error(SCRIPT_STRING_LITERAL("Expected 'end' after 'while' block"));
			return nullptr;
		}
		return m_graph.EmplaceNode<AST::Statement::While>(pCondition, pBody);
	}

	// repeat block until exp
	Optional<AST::Statement::Repeat*> Parser::StatementRepeat()
	{
		++m_state.loopDepth;

		State previousState = Move(m_state);
		m_state.pEnclosingState = previousState;
		AST::Statements stmtlist;
		ParseBlock(stmtlist);

		--m_state.loopDepth;
		if (UNLIKELY_ERROR(!Match(TokenType::Until)))
		{
			Error(SCRIPT_STRING_LITERAL("Expected 'until' after 'repeat'"));
			return nullptr;
		}
		Optional<AST::Expression::Base*> pCondition = Expression();

		TokenListType::const_iterator current = m_state.current;
		m_state = Move(previousState);
		m_state.current = current;

		return m_graph.EmplaceNode<AST::Statement::Repeat>(Move(stmtlist), pCondition);
	}

	// if exp then block {elseif exp then block} [else block] end
	Optional<AST::Statement::If*> Parser::StatementIf()
	{
		AST::Expressions conditions;
		conditions.EmplaceBack(*Expression());
		if (UNLIKELY_ERROR(!Match(TokenType::Then)))
		{
			Error(SCRIPT_STRING_LITERAL("Expected 'then' after 'if'"));
			return nullptr;
		}

		AST::Statements thens;
		{
			State previousState = Move(m_state);
			m_state.pEnclosingState = previousState;
			thens.EmplaceBack(*StatementBlock());
			TokenListType::const_iterator current = m_state.current;
			m_state = Move(previousState);
			m_state.current = current;
		}

		while (Match(TokenType::Elseif))
		{
			State previousState = Move(m_state);
			m_state.pEnclosingState = previousState;
			conditions.EmplaceBack(*Expression());
			if (UNLIKELY_ERROR(!Match(TokenType::Then)))
			{
				Error(SCRIPT_STRING_LITERAL("Expected 'then' after 'elseif'"));
				return nullptr;
			}
			thens.EmplaceBack(*StatementBlock());
			TokenListType::const_iterator current = m_state.current;
			m_state = Move(previousState);
			m_state.current = current;
		}

		Optional<AST::Statement::Base*> pElse;
		if (Match(TokenType::Else))
		{
			State previousState = Move(m_state);
			m_state.pEnclosingState = previousState;
			pElse = StatementBlock();
			TokenListType::const_iterator current = m_state.current;
			m_state = Move(previousState);
			m_state.current = current;
		}
		if (UNLIKELY_ERROR(!Match(TokenType::End)))
		{
			Error(SCRIPT_STRING_LITERAL("Expected 'end' after 'if', 'elseif', 'else'"));
			return nullptr;
		}
		return m_graph.EmplaceNode<AST::Statement::If>(Move(conditions), Move(thens), pElse);
	}

	[[nodiscard]] PrimitiveType GetPrimitiveType(const ArrayView<const Scripting::Type> types);

	[[nodiscard]] PrimitiveType GetPrimitiveType(const Scripting::Type& type)
	{
		if (type.Is<FloatType>() || type.Is<Math::Rotation2Df>() || type.Is<Math::Anglef>() || type.Is<Math::Densityf>() || type.Is<Math::Lengthf>() || type.Is<Math::Massf>() || type.Is<Math::Ratiof>() || type.Is<Math::RotationalSpeedf>() || type.Is<Math::Speedf>() || type.Is<Math::Torquef>())
		{
			return PrimitiveType::Float;
		}
		else if (type.Is<Math::Vector2f>())
		{
			return PrimitiveType::Float2;
		}
		else if (type.Is<Math::Vector3f>() || type.Is<Math::WorldCoordinate>())
		{
			return PrimitiveType::Float3;
		}
		else if (type.Is<Math::Vector4f>() || type.Is<Math::Color>() || type.Is<Math::Rotation3Df>())
		{
			return PrimitiveType::Float4;
		}
		else if (type.Is<IntegerType>())
		{
			return PrimitiveType::Integer;
		}
		else if (type.Is<Math::Vector2i>())
		{
			return PrimitiveType::Integer2;
		}
		else if (type.Is<Math::Vector3i>())
		{
			return PrimitiveType::Integer3;
		}
		else if (type.Is<Math::Vector4i>())
		{
			return PrimitiveType::Integer4;
		}
		else if (type.Is<Math::Vector2i::BoolType>() || type.Is<Math::Vector2f::BoolType>())
		{
			return PrimitiveType::Boolean2;
		}
		else if (type.Is<Math::Vector3i::BoolType>() || type.Is<Math::Vector3f::BoolType>())
		{
			return PrimitiveType::Boolean3;
		}
		else if (type.Is<Math::Vector4i::BoolType>() || type.Is<Math::Vector4f::BoolType>())
		{
			return PrimitiveType::Boolean4;
		}
		else if (type.Is<nullptr_type>())
		{
			return PrimitiveType::Null;
		}
		else if (type.Is<bool>())
		{
			return PrimitiveType::Boolean;
		}
		else if (type.Is<StringType>())
		{
			return PrimitiveType::String;
		}
		else if (type.Is<Reflection::DynamicTypeDefinition>())
		{
			const Reflection::DynamicTypeDefinition& typeDefinition = type.GetExpected<Reflection::DynamicTypeDefinition>();
			switch (typeDefinition.GetType())
			{
				case Reflection::TypeDefinitionType::Invalid:
				case Reflection::TypeDefinitionType::Native:
					ExpectUnreachable();
				case Reflection::TypeDefinitionType::Structure:
					return PrimitiveType::Any;
				case Reflection::TypeDefinitionType::Variant:
				{
					const ArrayView<const Reflection::DynamicPropertyInfo> variantFields = typeDefinition.GetVariantFields();
					Vector<Scripting::Type> variantTypes;
					variantTypes.Reserve(variantFields.GetSize());
					for (const Reflection::DynamicPropertyInfo& property : variantFields)
					{
						variantTypes.EmplaceBack(property.m_typeDefinition);
					}
					return GetPrimitiveType(variantTypes.GetView());
				}
			}
			ExpectUnreachable();
		}
		else
		{
			return PrimitiveType::Any;
		}
	}

	[[nodiscard]] PrimitiveType GetPrimitiveType(const ArrayView<const Scripting::Type> types)
	{
		Assert(types.HasElements());
		if (types.GetSize() == 1)
		{
			return GetPrimitiveType(types[0]);
		}

		PrimitiveType firstPrimitiveType = GetPrimitiveType(types[0]);
		// Convert types that can be cast to something else
		switch (firstPrimitiveType)
		{
			case PrimitiveType::Any:
				return PrimitiveType::Any;
			case PrimitiveType::Integer:
			case PrimitiveType::Integer2:
			case PrimitiveType::Integer3:
			case PrimitiveType::Integer4:
			case PrimitiveType::Float:
			case PrimitiveType::Float2:
			case PrimitiveType::Float3:
			case PrimitiveType::Float4:
			case PrimitiveType::Boolean2:
			case PrimitiveType::Boolean3:
			case PrimitiveType::Boolean4:
			case PrimitiveType::String:
				break;
			case PrimitiveType::Boolean:
			case PrimitiveType::Null:
				firstPrimitiveType = PrimitiveType::Integer;
		}
		for (const Scripting::Type& type : types + 1)
		{
			PrimitiveType primitiveType = GetPrimitiveType(type);
			switch (primitiveType)
			{
				case PrimitiveType::Any:
					return PrimitiveType::Any;
				case PrimitiveType::Integer:
				case PrimitiveType::Integer2:
				case PrimitiveType::Integer3:
				case PrimitiveType::Integer4:
				case PrimitiveType::Float:
				case PrimitiveType::Float2:
				case PrimitiveType::Float3:
				case PrimitiveType::Float4:
				case PrimitiveType::Boolean2:
				case PrimitiveType::Boolean3:
				case PrimitiveType::Boolean4:
				case PrimitiveType::String:
					break;
				case PrimitiveType::Boolean:
				case PrimitiveType::Null:
					primitiveType = PrimitiveType::Integer;
			}

			if (primitiveType != firstPrimitiveType)
			{
				return PrimitiveType::Any;
			}
		}

		return firstPrimitiveType;
	}

	// prefixexp ::= var | functioncall | ‘(’ exp ‘)’
	// var ::=  Name | prefixexp ‘[’ exp ‘]’ | prefixexp ‘.’ Name
	// functioncall :: = prefixexp args | prefixexp ‘:’ Name args
	// args ::=  ‘(’ [explist] ‘)’ | tableconstructor | LiteralString
	Optional<AST::Expression::Base*> Parser::Prefix()
	{
		Optional<AST::Expression::Base*> pPrefixExpression;
		if (Match(TokenType::LeftParentheses))
		{
			pPrefixExpression = Expression();
			if (UNLIKELY_ERROR(!Match(TokenType::RightParentheses)))
			{
				Error(SCRIPT_STRING_LITERAL("Expected ')' after '(' expression"));
				return nullptr;
			}
		}
		else if (Check(Array{
							 TokenType::Vec2i,
							 TokenType::Vec2f,
							 TokenType::Vec2b,
							 TokenType::Vec3i,
							 TokenType::Vec3f,
							 TokenType::Vec3b,
							 TokenType::Vec4i,
							 TokenType::Vec4f,
							 TokenType::Vec4b,
							 TokenType::Color,
							 TokenType::Rotation2D,
							 TokenType::Rotation3D,
							 TokenType::Asset,
							 TokenType::TextureAsset,
							 TokenType::Tag,
							 TokenType::ComponentSoftReference,
							 TokenType::RenderStage
						 }))
		{
			pPrefixExpression = Expression();

			// Handle constant function calls
			if (Check(TokenType::Period))
			{
				StateView functionCallState{m_state};
				// Ensure object state can't parse past the function call itself
				functionCallState.Match(TokenType::Period);

				if (functionCallState.Match(TokenType::Identifier))
				{
					const Token& functionToken = functionCallState.Previous();
					if (functionCallState.Match(TokenType::LeftParentheses))
					{
						m_state = functionCallState;
						if (pPrefixExpression.IsInvalid())
						{
							Error(SCRIPT_STRING_LITERAL("Expected valid object before function call"));
						}

						Optional<AST::Expression::Base*> pCalleeExpression = m_graph.EmplaceNode<AST::Expression::Variable>(
							pPrefixExpression,
							VariableToken{Token{functionToken}, Scripting::Type{Reflection::TypeDefinition::Get<Scripting::VM::DynamicFunction>()}}
						);

						AST::Expressions arglist;

						const Token& parentheses = Previous();
						if (!Check(TokenType::RightParentheses))
						{
							ParseExprlist(arglist);
							if (UNLIKELY_ERROR(arglist.GetSize() >= 255))
							{
								Error(SCRIPT_STRING_LITERAL("Expected less than 255 arguments in argslist"));
							}
						}
						if (UNLIKELY_ERROR(!Match(TokenType::RightParentheses)))
						{
							Error(SCRIPT_STRING_LITERAL("Expected ')' after '(' in argslist"));
						}
						return m_graph.EmplaceNode<AST::Expression::Call>(*pCalleeExpression, Move(arglist), parentheses.sourceLocation);
					}
				}
			}
		}
		else if (Match(Array{TokenType::Abs,           TokenType::Acos,        TokenType::Asin,    TokenType::Atan,
		                     TokenType::Ceil,          TokenType::CubicRoot,   TokenType::Cos,     TokenType::Deg,
		                     TokenType::Exp,           TokenType::Floor,       TokenType::Fract,   TokenType::InverseSqrt,
		                     TokenType::Log,           TokenType::Log2,        TokenType::Log10,   TokenType::MultiplicativeInverse,
		                     TokenType::Power2,        TokenType::Power10,     TokenType::Rad,     TokenType::Round,
		                     TokenType::Sign,          TokenType::SignNonZero, TokenType::Sin,     TokenType::Sqrt,
		                     TokenType::Tan,           TokenType::Truncate,    TokenType::Length,  TokenType::LengthSquared,
		                     TokenType::InverseLength, TokenType::Normalize,   TokenType::Inverse, TokenType::Right,
		                     TokenType::Forward,       TokenType::Up,          TokenType::Rotate,  TokenType::InverseRotate,
		                     TokenType::Euler,         TokenType::Any,         TokenType::All}))
		{
			const Token& prefix = Previous();
			if (UNLIKELY_ERROR(!Match(TokenType::LeftParentheses)))
			{
				Error(SCRIPT_STRING_LITERAL("Expected '(' after function name"));
				return {};
			}

			Optional<AST::Expression::Base*> pRightExpression = Expression(Invalid);
			if (pRightExpression.IsInvalid())
			{
				Error(SCRIPT_STRING_LITERAL("Expected expression"));
				return {};
			}

			const Types variableTypes = GetPossibleExpressionReturnTypes(*pRightExpression);
			const PrimitiveType primitiveType = GetPrimitiveType(variableTypes.GetView());
			pPrefixExpression = m_graph.EmplaceNode<AST::Expression::Unary>(Token(prefix), primitiveType, pRightExpression);

			if (UNLIKELY_ERROR(!Match(TokenType::RightParentheses)))
			{
				Error(SCRIPT_STRING_LITERAL("Expected ')' after function parlist"));
				return {};
			}

			return pPrefixExpression;
		}
		else if (Match(Array{
							 TokenType::Atan2,
							 TokenType::Mod,
							 TokenType::Max,
							 TokenType::Min,
							 TokenType::Power,
							 TokenType::AreNearlyEqual,
							 TokenType::Dot,
							 TokenType::Cross,
							 TokenType::Distance,
							 TokenType::Project,
							 TokenType::Reflect,
							 TokenType::Refract
						 }))
		{
			const Token& prefix = Previous();
			if (UNLIKELY_ERROR(!Match(TokenType::LeftParentheses)))
			{
				Error(SCRIPT_STRING_LITERAL("Expected '(' after function name"));
				return {};
			}

			Optional<AST::Expression::Base*> pLeftExpression = Expression(Invalid);
			if (pLeftExpression.IsInvalid())
			{
				Error(SCRIPT_STRING_LITERAL("Expected expression"));
				return {};
			}

			if (!Match(TokenType::Comma))
			{
				Error(SCRIPT_STRING_LITERAL("Expected comma"));
				return {};
			}

			Optional<AST::Expression::Base*> pRightExpression = Expression(Invalid);
			if (pRightExpression.IsInvalid())
			{
				Error(SCRIPT_STRING_LITERAL("Expected expression"));
				return {};
			}

			const Types variableTypes = GetPossibleExpressionReturnTypes(*pRightExpression);
			const PrimitiveType primitiveType = GetPrimitiveType(variableTypes.GetView());
			pPrefixExpression = m_graph.EmplaceNode<AST::Expression::Binary>(pLeftExpression, Token(prefix), primitiveType, pRightExpression);

			if (UNLIKELY_ERROR(!Match(TokenType::RightParentheses)))
			{
				Error(SCRIPT_STRING_LITERAL("Expected ')' after function parlist"));
				return {};
			}

			return pPrefixExpression;
		}
		else if (Match(TokenType::Random))
		{
			const Token& prefix = Previous();

			if (!Match(TokenType::LeftParentheses))
			{
				Error(SCRIPT_STRING_LITERAL("Expected ')' after function parlist"));
				return {};
			}

			Optional<AST::Expression::Base*> pLeftExpression;
			Optional<AST::Expression::Base*> pRightExpression;
			if (Match(TokenType::RightParentheses))
			{
				// Random value from 0 to 1
				pLeftExpression = m_graph.EmplaceNode<AST::Expression::Literal>(FloatType(0));
				pRightExpression = m_graph.EmplaceNode<AST::Expression::Literal>(FloatType(1));
			}
			else
			{
				pLeftExpression = Expression(Invalid);
				if (Match(TokenType::Comma))
				{
					pRightExpression = Expression(Invalid);
					if (pRightExpression.IsInvalid())
					{
						Error(SCRIPT_STRING_LITERAL("Expected expression"));
						return {};
					}
				}
				else
				{
					// Random value from 1 to the specified value
					pRightExpression = pLeftExpression;
					pLeftExpression = m_graph.EmplaceNode<AST::Expression::Literal>(FloatType(1));
				}

				if (UNLIKELY_ERROR(!Match(TokenType::RightParentheses)))
				{
					Error(SCRIPT_STRING_LITERAL("Expected ')' after function parlist"));
					return {};
				}
			}

			const Types variableTypes = GetPossibleExpressionReturnTypes(*pRightExpression);
			const PrimitiveType primitiveType = GetPrimitiveType(variableTypes.GetView());
			pPrefixExpression = m_graph.EmplaceNode<AST::Expression::Binary>(pLeftExpression, Token(prefix), primitiveType, pRightExpression);

			return pPrefixExpression;
		}
		else if (Match(TokenType::Identifier))
		{
			StateView objectState{m_state};
			objectState.Reverse();
			const Token& name = Previous();

			// Handle object function calls
			if (Check(TokenType::Period))
			{
				StateView functionCallState{m_state};
				// Ensure object state can't parse past the function call itself
				objectState.end = functionCallState.current;
				functionCallState.Match(TokenType::Period);

				if (functionCallState.Match(TokenType::Identifier))
				{
					const Token& functionToken = functionCallState.Previous();
					if (functionCallState.Match(TokenType::LeftParentheses))
					{
						// Emplace the object argument that came before the function call itself ("object.foo(args...)")
						m_state = objectState;
						Optional<AST::Expression::Base*> pObjectExpression = Expression();
						if (pObjectExpression.IsInvalid())
						{
							Error(SCRIPT_STRING_LITERAL("Expected valid object before function call"));
						}

						Optional<AST::Expression::Base*> pCalleeExpression = m_graph.EmplaceNode<AST::Expression::Variable>(
							pObjectExpression,
							VariableToken{Token{functionToken}, Scripting::Type{Reflection::TypeDefinition::Get<Scripting::VM::DynamicFunction>()}}
						);

						AST::Expressions arglist;

						const Token& parentheses = functionCallState.Previous();
						m_state = functionCallState;
						if (!Check(TokenType::RightParentheses))
						{
							ParseExprlist(arglist);
							if (UNLIKELY_ERROR(arglist.GetSize() >= 255))
							{
								Error(SCRIPT_STRING_LITERAL("Expected less than 255 arguments in argslist"));
							}
						}
						if (UNLIKELY_ERROR(!Match(TokenType::RightParentheses)))
						{
							Error(SCRIPT_STRING_LITERAL("Expected ')' after '(' in argslist"));
						}
						return m_graph.EmplaceNode<AST::Expression::Call>(*pCalleeExpression, Move(arglist), parentheses.sourceLocation);
					}
				}
			}

			if (Check(Array{TokenType::LeftParentheses, TokenType::LeftBrace, TokenType::String}))
			{
				pPrefixExpression = m_graph.EmplaceNode<AST::Expression::Variable>(
					VariableToken{Token{name}, Reflection::TypeDefinition::Get<VM::DynamicFunction>()}
				);
			}
			else
			{
				const DeclareVariableResult result{EmplaceGlobalVariableType(name.identifier, Reflection::TypeDefinition::Get<nullptr_type>())};
				if (result.wasDeclared)
				{
					const bool isLocal = false;
					pPrefixExpression =
						m_graph.EmplaceNode<AST::Expression::VariableDeclaration>(VariableToken{Token{name}, result.types.GetView()}, isLocal);
				}
				else
				{
					pPrefixExpression = m_graph.EmplaceNode<AST::Expression::Variable>(VariableToken{Token{name}, result.types.GetView()});
				}
			}
		}

		if (UNLIKELY_ERROR(pPrefixExpression.IsInvalid()))
		{
			Error(SCRIPT_STRING_LITERAL("Expected '(' or name prefix"));
			return nullptr;
		}

		while (!ReachedEnd())
		{
			if (Check(Array{TokenType::LeftParentheses, TokenType::LeftBrace, TokenType::String}))
			{
				AST::Expressions arglist;
				ParseArgslist(arglist);
				const Token& parentheses = Previous();
				pPrefixExpression = m_graph.EmplaceNode<AST::Expression::Call>(*pPrefixExpression, Move(arglist), parentheses.sourceLocation);
			}
			else if (Match(TokenType::LeftBracket))
			{
				// TODO: Stable array identifier?
				const Guid arrayIdentifier = Guid::Generate();
				[[maybe_unused]] const bool wasDeclared =
					EmplaceLocalVariableType(arrayIdentifier, Reflection::TypeDefinition::Get<ArrayVariableType>()).wasDeclared;
				Assert(wasDeclared);

				pPrefixExpression = m_graph.EmplaceNode<AST::Expression::Variable>(
					Move(pPrefixExpression),
					Expression(),
					VariableToken{Token{TokenType::Identifier, StringType{}, arrayIdentifier}, Reflection::TypeDefinition::Get<ArrayVariableType>()}
				);
				if (UNLIKELY_ERROR(!Match(TokenType::RightBracket)))
				{
					Error(SCRIPT_STRING_LITERAL("Expected ']' after '[' in prefix expression"));
					return nullptr;
				}
			}
			else if (Match(TokenType::Period))
			{
				if (UNLIKELY_ERROR(!Match(TokenType::Identifier)))
				{
					Error(SCRIPT_STRING_LITERAL("Expected identifier after '.' in prefix expression"));
					return nullptr;
				}
				const Token& name = Previous();
				pPrefixExpression = m_graph.EmplaceNode<AST::Expression::Variable>(
					pPrefixExpression,
					VariableToken{Token{name}, Reflection::TypeDefinition::Get<nullptr_type>()}
				);
			}
			else if (Match(TokenType::Colon))
			{
				// TODO(Ben): Implement prefixexp ‘:’ Name args
			}
			else
			{
				break;
			}
		}

		return pPrefixExpression;
	}

	Optional<const Types*> Parser::State::FindVariableTypes(const Guid variableIdentifier) const
	{
		auto it = m_localVariableTypes.Find(variableIdentifier);
		if (it != m_localVariableTypes.end())
		{
			return it->second;
		}
		else if (pEnclosingState.IsValid())
		{
			return pEnclosingState->FindVariableTypes(variableIdentifier);
		}
		else
		{
			return Invalid;
		}
	}

	Parser::DeclareVariableResult Parser::EmplaceGlobalVariableType(const Guid identifier, Types&& types)
	{
		auto it = m_globalVariableTypes.Find(identifier);
		if (it == m_globalVariableTypes.end())
		{
			if (Optional<const Types*> pAllowedTypes = m_state.FindVariableTypes(identifier))
			{
				return DeclareVariableResult{*pAllowedTypes, false};
			}

			it = m_globalVariableTypes.Emplace(Guid{identifier}, Forward<Types>(types));
			return DeclareVariableResult{it->second, true};
		}
		else
		{
			// Assert(m_globalVariableTypes.Find(identifier)->second.GetView() == types.GetView());
			return DeclareVariableResult{it->second, false};
		}
	}

	void Parser::UpdateGlobalVariableType(const Guid identifier, Types&& types)
	{
		auto variableTypeIt = m_globalVariableTypes.Find(identifier);
		Assert(variableTypeIt != m_globalVariableTypes.end());
		variableTypeIt->second = Forward<Types>(types);
	}

	const Types& Parser::DeclareLocalVariableType(const Guid identifier, Types&& types)
	{
		auto it = m_state.m_localVariableTypes.Find(identifier);
		if (it != m_state.m_localVariableTypes.end())
		{
			it->second = Forward<Types>(types);
		}
		else
		{
			it = m_state.m_localVariableTypes.Emplace(Guid{identifier}, Forward<Types>(types));
		}
		return it->second;
	}

	Parser::DeclareVariableResult Parser::EmplaceLocalVariableType(const Guid identifier, Types&& types)
	{
		if (Optional<const Types*> pAllowedTypes = m_state.FindVariableTypes(identifier))
		{
			return DeclareVariableResult{*pAllowedTypes, false};
		}

		const auto it = m_state.m_localVariableTypes.Emplace(Guid{identifier}, Forward<Types>(types));
		return DeclareVariableResult{it->second, true};
	}

	void Parser::UpdateLocalVariableType(const Guid identifier, Types&& types)
	{
		auto variableTypeIt = m_state.m_localVariableTypes.Find(identifier);
		Assert(variableTypeIt != m_state.m_localVariableTypes.end());
		variableTypeIt->second = Forward<Types>(types);
	}

	void Parser::UpdateVariableType(const Guid identifier, Types&& types)
	{
		auto localVariableTypeIt = m_state.m_localVariableTypes.Find(identifier);
		if (localVariableTypeIt != m_state.m_localVariableTypes.end())
		{
			localVariableTypeIt->second = Forward<Types>(types);
		}
		else
		{
			auto globalVariableTypeIt = m_globalVariableTypes.Find(identifier);
			if (globalVariableTypeIt != m_globalVariableTypes.end())
			{
				globalVariableTypeIt->second = Forward<Types>(types);
			}
		}
	}

	enum class TruthyType : uint8
	{
		Dynamic,
		Falsey,
		Truthy,
	};
	[[nodiscard]] TruthyType GetTruthyType(const ArrayView<const Scripting::Type> types)
	{
		if (types.GetSize() == 1)
		{
			if (types[0].Is<nullptr_type>())
			{
				return TruthyType::Falsey;
			}
			else if (types[0].Is<IntegerType>() || types[0].Is<Math::Vector4i>() || types[0].Is<FloatType>() || types[0].Is<Math::Vector4f>() || types[0].Is<StringType>() || types[0].Is<VM::DynamicFunction>())
			{
				return TruthyType::Truthy;
			}

			return TruthyType::Dynamic;
		}
		else
		{
			return TruthyType::Dynamic;
		}
	}

	[[nodiscard]] Types ResolveLogicalResultType(const TokenType operatorType, Types&& leftTypes, Types&& rightTypes)
	{
		switch (operatorType)
		{
			case TokenType::Or:
			{
				switch (GetTruthyType(leftTypes))
				{
					case TruthyType::Truthy:
						return Move(leftTypes);
					case TruthyType::Falsey:
					{
						if (leftTypes == rightTypes)
						{
							return Move(leftTypes);
						}
						switch (GetTruthyType(rightTypes))
						{
							case TruthyType::Truthy:
							case TruthyType::Falsey:
								return Move(rightTypes);
							case TruthyType::Dynamic:
							{
								leftTypes.Reserve(leftTypes.GetSize() + rightTypes.GetSize());
								for (Scripting::Type& type : rightTypes)
								{
									leftTypes.EmplaceBackUnique(Move(type));
								}
								return Move(leftTypes);
							}
						}
						ExpectUnreachable();
					}
					case TruthyType::Dynamic:
					{
						leftTypes.Reserve(leftTypes.GetSize() + rightTypes.GetSize());
						for (Reflection::TypeDefinition type : rightTypes)
						{
							leftTypes.EmplaceBackUnique(Move(type));
						}
						return Move(leftTypes);
					}
				}
			}
			case TokenType::And:
			{
				switch (GetTruthyType(leftTypes))
				{
					case TruthyType::Falsey:
						return Move(leftTypes);
					case TruthyType::Truthy:
					{
						if (leftTypes == rightTypes)
						{
							return Move(leftTypes);
						}
						switch (GetTruthyType(rightTypes))
						{
							case TruthyType::Truthy:
							case TruthyType::Falsey:
								return Move(rightTypes);
							case TruthyType::Dynamic:
							{
								leftTypes.Reserve(leftTypes.GetSize() + rightTypes.GetSize());
								for (Scripting::Type& type : rightTypes)
								{
									leftTypes.EmplaceBackUnique(Move(type));
								}
								return Move(leftTypes);
							}
						}
						ExpectUnreachable();
					}
					case TruthyType::Dynamic:
					{
						leftTypes.Reserve(leftTypes.GetSize() + rightTypes.GetSize());
						for (Reflection::TypeDefinition type : rightTypes)
						{
							leftTypes.EmplaceBackUnique(Move(type));
						}
						return Move(leftTypes);
					}
				}
			}
			default:
				ExpectUnreachable();
		}
	}

	[[nodiscard]] PrimitiveType ResolveBinaryPrimitiveType(const TokenType type, const PrimitiveType left, const PrimitiveType right)
	{
		switch (type)
		{
			case TokenType::Plus:
			case TokenType::Minus:
			case TokenType::Star:
			case TokenType::Slash:
			case TokenType::Percent:
			{
				if (left == right)
				{
					return left;
				}

				return PrimitiveType::Any;
			}

			case TokenType::Less:
			case TokenType::LessEqual:
			case TokenType::Greater:
			case TokenType::GreaterEqual:
			case TokenType::EqualEqual:
			case TokenType::NotEqual:
			case TokenType::LessLess:
			case TokenType::GreaterGreater:
			{
				if (left == right)
				{
					return left;
				}
				if (left == PrimitiveType::Null)
				{
					return right;
				}
				if (right == PrimitiveType::Null)
				{
					return left;
				}

				if (left == PrimitiveType::Any || right == PrimitiveType::Any)
				{
					return PrimitiveType::Any;
				}

				if ((left == PrimitiveType::Integer && right == PrimitiveType::Float) || (right == PrimitiveType::Integer && left == PrimitiveType::Float))
				{
					Assert(false, "TODO: Cast integer to float");
					return PrimitiveType::Float;
				}

				return PrimitiveType::Integer;
			}

			default:
				ExpectUnreachable();
		}
	}

	[[nodiscard]] Types Parser::GetPossibleExpressionReturnTypes(const AST::Expression::Base& expression) const
	{
		switch (expression.GetType())
		{
			case AST::NodeType::Variable:
			{
				const AST::Expression::Variable& variableExpression = static_cast<const AST::Expression::Variable&>(expression);
				return variableExpression.GetIdentifier().m_types.GetView();
			}
			case AST::NodeType::VariableDeclaration:
			{
				const AST::Expression::VariableDeclaration& variableExpression = static_cast<const AST::Expression::VariableDeclaration&>(expression
				);
				return variableExpression.GetIdentifier().m_types.GetView();
			}
			case AST::NodeType::Literal:
				// Convert to ScriptValue and back to resolve intrinsically known types
				return ScriptValue{static_cast<const AST::Expression::Literal&>(expression).GetValue()}.ToAny().GetTypeDefinition();

			case AST::NodeType::Call:
			{
				// Get the function return value
				const AST::Expression::Call& callExpression = static_cast<const AST::Expression::Call&>(expression);
				const AST::Expression::Base& callee = *callExpression.GetCallee();

				Assert(callee.GetType() == AST::NodeType::Variable);
				const AST::Expression::Variable& calleeVariable = static_cast<const AST::Expression::Variable&>(callee);

				auto it = m_state.m_functionReturnTypes.Find(calleeVariable.GetIdentifier().identifier);
				if (it != m_state.m_functionReturnTypes.end())
				{
					return it->second.GetView();
				}
				else
				{
					// Externally defined function
					Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
					if (const Optional<const Reflection::FunctionInfo*> pFunctionInfo = reflectionRegistry.FindFunctionDefinition(calleeVariable.GetIdentifier().identifier))
					{
						return pFunctionInfo->m_returnType.m_type;
					}
					else
					{
						return Reflection::TypeDefinition::Get<Any>();
					}
				}
			}

			case AST::NodeType::Binary:
			{
				const AST::Expression::Binary& binaryExpression = static_cast<const AST::Expression::Binary&>(expression);
				switch (binaryExpression.GetOperator().type)
				{
					case TokenType::Plus:
					case TokenType::Minus:
					case TokenType::Star:
					case TokenType::Slash:
					case TokenType::Percent:
					case TokenType::LessLess:
					case TokenType::GreaterGreater:
					case TokenType::Atan2:
					case TokenType::Mod:
					case TokenType::Max:
					case TokenType::Min:
					case TokenType::Power:
					case TokenType::Random:
					case TokenType::Cross:
					case TokenType::Project:
					case TokenType::Reflect:
					case TokenType::Refract:
					{
						Types leftTypes = GetPossibleExpressionReturnTypes(*binaryExpression.GetLeft());
						Types rightTypes = GetPossibleExpressionReturnTypes(*binaryExpression.GetRight());
						if (leftTypes.GetView() == rightTypes.GetView())
						{
							return Move(leftTypes);
						}

						return Reflection::TypeDefinition::Get<Any>();
					}

					case TokenType::Dot:
					case TokenType::Distance:
						return Reflection::TypeDefinition::Get<FloatType>();

					case TokenType::Less:
					case TokenType::LessEqual:
					case TokenType::Greater:
					case TokenType::GreaterEqual:
					case TokenType::Equal:
					case TokenType::NotEqual:
					case TokenType::EqualEqual:
					case TokenType::Any:
					case TokenType::All:
					{
						return Reflection::TypeDefinition::Get<bool>();
					}

					default:
						ExpectUnreachable();
				}
			}

			case AST::NodeType::Unary:
			{
				const AST::Expression::Unary& unaryExpression = static_cast<const AST::Expression::Unary&>(expression);
				switch (unaryExpression.GetOperator().type)
				{
					case TokenType::Minus:
					case TokenType::Abs:
					case TokenType::Acos:
					case TokenType::Asin:
					case TokenType::Atan:
					case TokenType::Ceil:
					case TokenType::CubicRoot:
					case TokenType::Cos:
					case TokenType::Deg:
					case TokenType::Exp:
					case TokenType::Floor:
					case TokenType::Fract:
					case TokenType::InverseSqrt:
					case TokenType::Log:
					case TokenType::Log2:
					case TokenType::Log10:
					case TokenType::MultiplicativeInverse:
					case TokenType::Power2:
					case TokenType::Power10:
					case TokenType::Rad:
					case TokenType::Round:
					case TokenType::Sign:
					case TokenType::SignNonZero:
					case TokenType::Sin:
					case TokenType::Sqrt:
					case TokenType::Tan:
					case TokenType::AreNearlyEqual:
					case TokenType::Normalize:
					case TokenType::Inverse:
					case TokenType::Right:
					case TokenType::Forward:
					case TokenType::Up:
					case TokenType::Rotate:
					case TokenType::InverseRotate:
					case TokenType::Euler:
						return GetPossibleExpressionReturnTypes(*unaryExpression.GetRight());
					case TokenType::Truncate:
						return Reflection::TypeDefinition::Get<IntegerType>();

					case TokenType::Length:
					case TokenType::LengthSquared:
					case TokenType::InverseLength:
						return Reflection::TypeDefinition::Get<FloatType>();

					case TokenType::Not:
					case TokenType::Exclamation:
					case TokenType::Any:
					case TokenType::All:
						return Reflection::TypeDefinition::Get<bool>();

					default:
						ExpectUnreachable();
				}
			}

			case AST::NodeType::Logical:
			{
				const AST::Expression::Logical& logicalExpression = static_cast<const AST::Expression::Logical&>(expression);
				Types leftTypes = GetPossibleExpressionReturnTypes(*logicalExpression.GetLeft());
				Types rightTypes = GetPossibleExpressionReturnTypes(*logicalExpression.GetRight());
				return ResolveLogicalResultType(logicalExpression.GetOperator().type, Move(leftTypes), Move(rightTypes));
			}

			case AST::NodeType::Function:
			{
				return Reflection::TypeDefinition::Get<Any>();
			}

			// Inapplicable types
			case AST::NodeType::Group:
			case AST::NodeType::Assignment:

			case AST::NodeType::Expression:
			case AST::NodeType::Block:
			case AST::NodeType::If:
			case AST::NodeType::While:
			case AST::NodeType::Repeat:
			case AST::NodeType::Break:
			case AST::NodeType::Return:
			case AST::NodeType::Count:
				ExpectUnreachable();
		}
		ExpectUnreachable();
	}

	Any Token::ToAny() const
	{
		switch (type)
		{
			case TokenType::Null:
				return nullptr;
			case TokenType::True:
				return true;
			case TokenType::False:
				return false;
			case TokenType::Number:
			{
				if (literal.Contains(StringType::CharType('.')))
				{
					return FloatType(literal.GetView().ToDouble());
				}
				else
				{
					return literal.GetView().ToIntegral<IntegerType>();
				}
			}
			case TokenType::Float:
			{
				return FloatType(literal.GetView().ToDouble());
			}
			case TokenType::Integer:
			{
				return literal.GetView().ToIntegral<IntegerType>();
			}
			case TokenType::String:
			{
				// Remove first and last '"' or ','
				StringType::ConstView stringView = literal.GetView();
				if (stringView[0] == '"' || stringView[0] == '\'' || stringView[0] == ',')
				{
					stringView++;
				}
				if (stringView.GetLastElement() == '"' || stringView.GetLastElement() == '\'' || stringView.GetLastElement() == ',')
				{
					stringView--;
				}
				return StringType(stringView);
			}
			default:
				return {};
		}
	}

	// exp ::= nil | false | true | Numeral | LiteralString | ‘...’ | functiondef |
	//		   prefixexp | tableconstructor | exp binop exp | unop exp
	// Modified Pratt Parser
	Optional<AST::Expression::Base*> Parser::Expression(const Optional<const AST::Expression::Base*> pVariableExpression, uint8 precedence)
	{
		Optional<AST::Expression::Base*> pExpression;
		if (Match(Array{
					TokenType::Null,
					TokenType::False,
					TokenType::True,
					TokenType::Number,
					TokenType::Float,
					TokenType::Integer,
					TokenType::String
				}))
		{
			pExpression = m_graph.EmplaceNode<AST::Expression::Literal>(Previous().ToAny());
		}
		else if (Match(TokenType::Pi))
		{
			pExpression = m_graph.EmplaceNode<AST::Expression::Literal>(Math::TConstants<FloatType>::PI);
		}
		else if (Match(TokenType::Pi2))
		{
			pExpression = m_graph.EmplaceNode<AST::Expression::Literal>(Math::TConstants<FloatType>::PI2);
		}
		else if (Match(TokenType::e))
		{
			pExpression = m_graph.EmplaceNode<AST::Expression::Literal>(Math::TConstants<FloatType>::e);
		}
		else if (Check(Array{
							 TokenType::Vec2i,
							 TokenType::Vec2f,
							 TokenType::Vec2b,
							 TokenType::Vec3i,
							 TokenType::Vec3f,
							 TokenType::Vec3b,
							 TokenType::Vec4i,
							 TokenType::Vec4f,
							 TokenType::Vec4b,
							 TokenType::Color,
							 TokenType::Rotation2D,
							 TokenType::Rotation3D,
							 TokenType::Asset,
							 TokenType::TextureAsset,
							 TokenType::Tag,
							 TokenType::ComponentSoftReference,
							 TokenType::RenderStage
						 }))
		{
			const TokenType typeToken = Peek().type;
			Match(typeToken);
			pExpression = BraceInitializer(typeToken);
		}
		else if (Match(TokenType::Function))
		{
			State previousState = Move(m_state);
			m_state.pEnclosingState = previousState;
			AST::Expression::Function functionBody = FunctionBody();
			TokenListType::const_iterator current = m_state.current;
			m_state = Move(previousState);
			m_state.current = current;

			pExpression = m_graph.EmplaceNode<AST::Expression::Function>(Move(functionBody));
		}
		else if (Match(TokenType::DotDotDot))
		{
			// TODO(Ben): Implement '...'
		}
		else if (Check(TokenType::LeftParentheses))
		{
			pExpression = Prefix();
		}
		else if (Check(TokenType::LeftBrace))
		{
			if (pVariableExpression.IsInvalid())
			{
				Error("Expected variable prior to brace initialization");
				return Invalid;
			}

			pExpression = BraceInitializer(*pVariableExpression);
		}
		else if (Match(Array{TokenType::Minus, TokenType::Not, TokenType::Hashtag, TokenType::Tilde}))
		{
			const Token& prefix = Previous();
			const uint8 prefixPrecedence = uint8(GetPrefixPrecedence(prefix.type));
			if (Optional<AST::Expression::Base*> pRightExpression = Expression(Invalid, prefixPrecedence))
			{
				const Types variableTypes = GetPossibleExpressionReturnTypes(*pRightExpression);
				const PrimitiveType primitiveType = GetPrimitiveType(variableTypes.GetView());
				pExpression = m_graph.EmplaceNode<AST::Expression::Unary>(Token(prefix), primitiveType, pRightExpression);
			}
		}
		else if (Check(TokenType::Identifier))
		{
			pExpression = Prefix();
		}
		else if (Check(Array{
							 TokenType::Abs,
							 TokenType::Acos,
							 TokenType::Asin,
							 TokenType::Atan,
							 TokenType::Ceil,
							 TokenType::CubicRoot,
							 TokenType::Cos,
							 TokenType::Deg,
							 TokenType::Exp,
							 TokenType::Floor,
							 TokenType::Fract,
							 TokenType::InverseSqrt,
							 TokenType::Log,
							 TokenType::Log2,
							 TokenType::Log10,
							 TokenType::MultiplicativeInverse,
							 TokenType::Power2,
							 TokenType::Power10,
							 TokenType::Rad,
							 TokenType::Round,
							 TokenType::Sign,
							 TokenType::SignNonZero,
							 TokenType::Sin,
							 TokenType::Sqrt,
							 TokenType::Tan,
							 TokenType::Truncate,
							 TokenType::AreNearlyEqual,
							 TokenType::Atan2,
							 TokenType::Mod,
							 TokenType::Max,
							 TokenType::Min,
							 TokenType::Power,
							 TokenType::Random,
							 TokenType::AreNearlyEqual,
							 TokenType::Dot,
							 TokenType::Cross,
							 TokenType::Distance,
							 TokenType::Length,
							 TokenType::LengthSquared,
							 TokenType::InverseLength,
							 TokenType::Normalize,
							 TokenType::Project,
							 TokenType::Reflect,
							 TokenType::Refract,
							 TokenType::Inverse,
							 TokenType::Right,
							 TokenType::Forward,
							 TokenType::Up,
							 TokenType::Rotate,
							 TokenType::InverseRotate,
							 TokenType::Euler,
							 TokenType::Any,
							 TokenType::All
						 }))
		{
			pExpression = Prefix();
		}

		while (!ReachedEnd() && precedence < GetInfixPrecedence(Peek().type))
		{
			Advance();
			const Token& infix = Previous();
			if (infix.type == TokenType::Or || infix.type == TokenType::And)
			{
				pExpression = m_graph.EmplaceNode<AST::Expression::Logical>(
					pExpression,
					Token(infix),
					Expression(Invalid, uint8(GetInfixPrecedence(infix.type)))
				);
			}
			else
			{
				const Types leftVariableTypes = GetPossibleExpressionReturnTypes(*pExpression);
				const PrimitiveType leftPrimitiveType = GetPrimitiveType(leftVariableTypes.GetView());

				Optional<AST::Expression::Base*> pRightExpression = Expression(Invalid, uint8(GetInfixPrecedence(infix.type)));
				const Types rightVariableTypes = GetPossibleExpressionReturnTypes(*pRightExpression);
				const PrimitiveType rightPrimitiveType = GetPrimitiveType(rightVariableTypes.GetView());

				const TokenType type = infix.type;
				pExpression = m_graph.EmplaceNode<AST::Expression::Binary>(
					pExpression,
					Token(infix),
					ResolveBinaryPrimitiveType(type, leftPrimitiveType, rightPrimitiveType),
					Move(pRightExpression)
				);
			}
		}

		if (Previous().type != TokenType::Return && pExpression.IsInvalid())
		{
			Error(SCRIPT_STRING_LITERAL("Expected one of these expressions 'nil', 'false', 'true', Numeral, LiteralString, '...', functiondef, "
			                            "prefixexp, tableconstructor, exp binop exp, unop exp."));
		}

		return pExpression;
	}

	// funcbody ::= ‘(’ [parlist] ‘)’ block end
	// parlist ::= namelist [‘,’ ‘...’] | ‘...’
	// namelist :: = Name{‘,’ Name}
	AST::Expression::Function Parser::FunctionBody()
	{
		if (UNLIKELY_ERROR(!Match(TokenType::LeftParentheses)))
		{
			Error(SCRIPT_STRING_LITERAL("Expected '(' after function name"));
			return {};
		}

		if (Match(TokenType::DotDotDot))
		{
			// TODO(Ben): Implement varargs '...'
		}

		AST::Expression::Function::Parameters parlist;
		if (!Check(TokenType::RightParentheses))
		{
			ParseNamelist(parlist);
			if (Match(TokenType::Comma))
			{
				if (UNLIKELY_ERROR(!Match(TokenType::DotDotDot)))
				{
					Error(SCRIPT_STRING_LITERAL("Expected '...' after parlist with trailing ','"));
				}
				// TODO(Ben): Implement trailing varargs
			}
		}

		if (UNLIKELY_ERROR(!Match(TokenType::RightParentheses)))
		{
			Error(SCRIPT_STRING_LITERAL("Expected ')' after function parlist"));
			return {};
		}

		AST::Expression::Variable::Tokens returnTypeTokens;
		if (Match(TokenType::Colon))
		{
			if (Match(TokenType::LeftParentheses))
			{
				ParseReturnTypes(returnTypeTokens);

				if (UNLIKELY_ERROR(!Match(TokenType::RightParentheses)))
				{
					Error(SCRIPT_STRING_LITERAL("Expected ')' after function return type"));
					return {};
				}
			}
			else
			{
				ParseReturnTypes(returnTypeTokens);
			}
		}

		for (VariableToken& parameterName : parlist)
		{
			if (m_state.m_localVariableTypes.Contains(parameterName.identifier))
			{
				Error(SCRIPT_STRING_LITERAL("Shadowing variable"));
				return {};
			}
			m_state.m_localVariableTypes.Emplace(Guid{parameterName.identifier}, parameterName.m_types.GetView());
		}

		AST::Statements stmtlist;
		ParseBlock(stmtlist);

		if (UNLIKELY_ERROR(!Match(TokenType::End)))
		{
			Error(SCRIPT_STRING_LITERAL("Expected 'end' after function body"));
			return {};
		}

		if (stmtlist.HasElements() && stmtlist.GetLastElement()->GetType() == AST::NodeType::Return && returnTypeTokens.IsEmpty())
		{
			returnTypeTokens.EmplaceBack(VariableToken{Token{}, Reflection::TypeDefinition::Get<nullptr_type>()});
			// Error(SCRIPT_STRING_LITERAL("No return type specified, but using 'return' keyword"));
			// return nullptr;
		}

		return AST::Expression::Function{Move(parlist), Move(returnTypeTokens), Move(stmtlist)};
	}

	Optional<AST::Expression::Base*> Parser::BraceInitializer(const AST::Expression::Base& variableExpression)
	{
		if (variableExpression.GetType() != AST::NodeType::VariableDeclaration)
		{
			Error("Expected variable declaration prior to brace initialization");
			return Invalid;
		}

		const AST::Expression::VariableDeclaration& variableDeclaration =
			static_cast<const AST::Expression::VariableDeclaration&>(variableExpression);
		if (variableDeclaration.GetIdentifier().m_types.GetSize() != 1)
		{
			Error("Brace initialization requires known type");
			return Invalid;
		}

		const Scripting::Type& variableType = variableDeclaration.GetIdentifier().m_types[0];
		const TokenType variableTypeToken = GetTokenType(variableType);
		return BraceInitializer(variableTypeToken);
	}

	Optional<AST::Expression::Base*> Parser::BraceInitializer(const TokenType variableType)
	{
		if (UNLIKELY_ERROR(!Match(TokenType::LeftBrace)))
		{
			Error(SCRIPT_STRING_LITERAL("Expected '{' as start of brace initializer"));
			return Invalid;
		}

		if (Check(TokenType::RightBrace))
		{
			Error("Brace initialization requires values");
			return Invalid;
		}

		InlineVector<Any, 4> values;

		while (!Check(TokenType::RightBrace))
		{
			if (Match(Array{TokenType::Float, TokenType::Integer, TokenType::String}))
			{
				values.EmplaceBack(Previous().ToAny());
			}
			else
			{
				Error("Expected numeric type in brace initialization");
				return Invalid;
			}

			Match(TokenType::Comma);
		}
		if (values.IsEmpty())
		{
			Error("Expected value in brace initialization");
			return Invalid;
		}

		const Math::Range<uint32> expectedValueRange = [this](const TokenType type)
		{
			switch (type)
			{
				case TokenType::Rotation2D:
				case TokenType::TextureAsset:
				case TokenType::Tag:
				case TokenType::RenderStage:
					return Math::Range<uint32>::Make(1, 1);
				case TokenType::Asset:
					// Assets can support either 1 argument (asset guid) or 2 (asset guid and asset type)
					return Math::Range<uint32>::Make(1, 2);
				case TokenType::Vec2i:
				case TokenType::Vec2f:
				case TokenType::Vec2b:
				case TokenType::ComponentSoftReference:
					return Math::Range<uint32>::Make(2, 1);
				case TokenType::Vec3i:
				case TokenType::Vec3f:
				case TokenType::Vec3b:
					return Math::Range<uint32>::Make(3, 1);
				case TokenType::Rotation3D:
					return Math::Range<uint32>::Make(3, 2);
				case TokenType::Vec4i:
				case TokenType::Vec4f:
				case TokenType::Vec4b:
					return Math::Range<uint32>::Make(4, 1);
				case TokenType::Color:
					//  Allow 3 - 4 values
					return Math::Range<uint32>::Make(3, 2);
				default:
					Error("Unexpected brace initialized type");
					return Math::Range<uint32>::Make(0, 0);
			}
		}(variableType);

		if (!expectedValueRange.Contains(values.GetSize()))
		{
			Error("Value count mismatch in brace initialization");
			return Invalid;
		}

		static const auto getFloatValue = [](const ConstAnyView anyValue)
		{
			if (anyValue.Is<IntegerType>())
			{
				return (FloatType)anyValue.GetExpected<IntegerType>();
			}
			else
			{
				return anyValue.GetExpected<FloatType>();
			}
		};
		static const auto canConvertToFloat = [](const ConstAnyView anyValue)
		{
			return anyValue.Is<IntegerType>() || anyValue.Is<FloatType>();
		};

		Any initializedValue = [this, &values](const TokenType type) -> Any
		{
			switch (type)
			{
				case TokenType::Rotation2D:
				{
					if (!values[0].Is<FloatType>())
					{
						Error("Value type mismatch in brace initialization");
						return {};
					}
					return Math::Rotation2Df{Math::Anglef::FromDegrees(values[0].GetExpected<FloatType>())};
				}
				case TokenType::Asset:
				{
					if (!values[0].Is<StringType>())
					{
						Error("Value type mismatch in brace initialization");
						return {};
					}

					const Asset::Guid assetGuid = Guid::TryParse(values[0].GetExpected<StringType>());
					if (assetGuid.IsInvalid())
					{
						Error("Invalid asset guid in brace initialization");
						return {};
					}

					if (values.GetSize() == 2)
					{
						const Asset::TypeGuid assetTypeGuid = Guid::TryParse(values[1].GetExpected<StringType>());
						if (assetTypeGuid.IsInvalid())
						{
							Error("Invalid asset type guid in brace initialization");
							return {};
						}

						return Asset::Reference{assetGuid, assetTypeGuid};
					}
					else
					{
						return Asset::Guid{assetGuid};
					}
				}
				case TokenType::TextureAsset:
				{
					if (!values[0].Is<StringType>())
					{
						Error("Value type mismatch in brace initialization");
						return {};
					}

					const Asset::Guid assetGuid = Guid::TryParse(values[0].GetExpected<StringType>());
					if (assetGuid.IsInvalid())
					{
						Error("Invalid asset guid in brace initialization");
						return {};
					}

					return Rendering::TextureGuid{assetGuid};
				}
				case TokenType::Tag:
				{
					if (!values[0].Is<StringType>())
					{
						Error("Value type mismatch in brace initialization");
						return {};
					}

					const Guid tagGuid = Guid::TryParse(values[0].GetExpected<StringType>());
					if (tagGuid.IsInvalid())
					{
						Error("Invalid tag guid in brace initialization");
						return {};
					}

					return Tag::Guid{tagGuid};
				}
				case TokenType::RenderStage:
				{
					if (!values[0].Is<StringType>())
					{
						Error("Value type mismatch in brace initialization");
						return {};
					}

					const Guid stageGuid = Guid::TryParse(values[0].GetExpected<StringType>());
					if (stageGuid.IsInvalid())
					{
						Error("Invalid render stage guid in brace initialization");
						return {};
					}

					return Rendering::StageGuid{stageGuid};
				}
				case TokenType::ComponentSoftReference:
				{
					if (!values[0].Is<StringType>() || !values[1].Is<StringType>())
					{
						Error("Value type mismatch in brace initialization");
						return {};
					}

					const Guid componentTypeGuid = Guid::TryParse(values[0].GetExpected<StringType>());
					const Guid componentInstanceGuid = Guid::TryParse(values[1].GetExpected<StringType>());

					Entity::ComponentTypeIdentifier componentTypeIdentifier;
					const Optional<Entity::Manager*> pEntityManager = System::Find<Entity::Manager>();
					Assert(pEntityManager.IsValid());
					if (LIKELY(pEntityManager.IsValid()))
					{
						componentTypeIdentifier = pEntityManager->GetRegistry().FindIdentifier(componentTypeGuid);
					}

					return Entity::ComponentSoftReference{componentTypeIdentifier, Entity::ComponentSoftReference::Instance{componentInstanceGuid}};
				}
				case TokenType::Vec2i:
				{
					if (!values.GetView().All(
								[](const ConstAnyView value)
								{
									return value.Is<IntegerType>();
								}
							))
					{
						Error("Value type mismatch in brace initialization");
						return {};
					}
					return Math::Vector2i{values[0].GetExpected<IntegerType>(), values[1].GetExpected<IntegerType>()};
				}
				case TokenType::Vec2f:
				{
					if (!values.GetView().All(
								[](const ConstAnyView value)
								{
									return canConvertToFloat(value);
								}
							))
					{
						Error("Value type mismatch in brace initialization");
						return {};
					}
					return Math::Vector2f{getFloatValue(values[0]), getFloatValue(values[1])};
				}
				case TokenType::Vec2b:
				{
					if (!values.GetView().All(
								[](const ConstAnyView value)
								{
									return value.Is<bool>();
								}
							))
					{
						Error("Value type mismatch in brace initialization");
						return {};
					}
					return Math::Vector2i::BoolType{
						values[0].GetExpected<bool>() ? (int32)0xFFFFFFFF : 0,
						values[1].GetExpected<bool>() ? (int32)0xFFFFFFFF : 0
					};
				}
				case TokenType::Vec3i:
				{
					if (!values.GetView().All(
								[](const ConstAnyView value)
								{
									return value.Is<IntegerType>();
								}
							))
					{
						Error("Value type mismatch in brace initialization");
						return {};
					}
					return Math::Vector3i{
						values[0].GetExpected<IntegerType>(),
						values[1].GetExpected<IntegerType>(),
						values[2].GetExpected<IntegerType>()
					};
				}
				case TokenType::Vec3f:
				{
					if (!values.GetView().All(
								[](const ConstAnyView value)
								{
									return canConvertToFloat(value);
								}
							))
					{
						Error("Value type mismatch in brace initialization");
						return {};
					}
					return Math::Vector3f{getFloatValue(values[0]), getFloatValue(values[1]), getFloatValue(values[2])};
				}
				case TokenType::Vec3b:
				{
					if (!values.GetView().All(
								[](const ConstAnyView value)
								{
									return value.Is<bool>();
								}
							))
					{
						Error("Value type mismatch in brace initialization");
						return {};
					}
					return Math::Vector3i::BoolType{
						values[0].GetExpected<bool>() ? (int32)0xFFFFFFFF : 0,
						values[1].GetExpected<bool>() ? (int32)0xFFFFFFFF : 0,
						values[2].GetExpected<bool>() ? (int32)0xFFFFFFFF : 0
					};
				}
				case TokenType::Rotation3D:
				{
					if (!values.GetView().All(
								[](const ConstAnyView value)
								{
									return canConvertToFloat(value);
								}
							))
					{
						Error("Value type mismatch in brace initialization");
						return {};
					}
					switch (values.GetSize())
					{
						case 3:
							return Math::Rotation3Df{Math::EulerAnglesf::FromDegrees(
								Math::Vector3f{getFloatValue(values[0]), getFloatValue(values[1]), getFloatValue(values[2])}
							)};
						case 4:
							return Math::Rotation3Df{
								getFloatValue(values[0]),
								getFloatValue(values[1]),
								getFloatValue(values[2]),
								getFloatValue(values[3])
							};
					}
				}
				case TokenType::Vec4i:
				{
					if (!values.GetView().All(
								[](const ConstAnyView value)
								{
									return value.Is<IntegerType>();
								}
							))
					{
						Error("Value type mismatch in brace initialization");
						return {};
					}
					return Math::Vector4i{
						values[0].GetExpected<IntegerType>(),
						values[1].GetExpected<IntegerType>(),
						values[2].GetExpected<IntegerType>(),
						values[3].GetExpected<IntegerType>()
					};
				}
				case TokenType::Vec4f:
				{
					if (!values.GetView().All(
								[](const ConstAnyView value)
								{
									return canConvertToFloat(value);
								}
							))
					{
						Error("Value type mismatch in brace initialization");
						return {};
					}
					return Math::Vector4f{getFloatValue(values[0]), getFloatValue(values[1]), getFloatValue(values[2]), getFloatValue(values[3])};
				}
				case TokenType::Vec4b:
				{
					if (!values.GetView().All(
								[](const ConstAnyView value)
								{
									return value.Is<bool>();
								}
							))
					{
						Error("Value type mismatch in brace initialization");
						return {};
					}
					return Math::Vector4i::BoolType{
						values[0].GetExpected<bool>() ? (int32)0xFFFFFFFF : 0,
						values[1].GetExpected<bool>() ? (int32)0xFFFFFFFF : 0,
						values[2].GetExpected<bool>() ? (int32)0xFFFFFFFF : 0,
						values[3].GetExpected<bool>() ? (int32)0xFFFFFFFF : 0
					};
				}
				case TokenType::Color:
				{
					if (!values.GetView().All(
								[](const ConstAnyView value)
								{
									return canConvertToFloat(value);
								}
							))
					{
						Error("Value type mismatch in brace initialization");
						return {};
					}
					switch (values.GetSize())
					{
						case 3:
							return Math::Color{getFloatValue(values[0]), getFloatValue(values[1]), getFloatValue(values[2]), 1.f};
						case 4:
							return Math::Color{getFloatValue(values[0]), getFloatValue(values[1]), getFloatValue(values[2]), getFloatValue(values[3])};
						default:
							ExpectUnreachable();
					}
				}
				default:
					Error("Unexpected brace initialized type");
					return Any{};
			}
		}(variableType);

		Optional<AST::Expression::Base*> pExpression = m_graph.EmplaceNode<AST::Expression::Literal>(Move(initializedValue));

		if (UNLIKELY_ERROR(!Match(TokenType::RightBrace)))
		{
			Error(SCRIPT_STRING_LITERAL("Expected '}' after brace initialization"));
			return nullptr;
		}

		return pExpression;
	}

	// block ::= {stat} [retstat]
	void Parser::ParseBlock(AST::Statements& stmtlist)
	{
		while (!ReachedEnd() && !Check(Array{TokenType::End, TokenType::Until, TokenType::Elseif, TokenType::Else, TokenType::Return}))
		{
			if (Optional<AST::Statement::Base*> pStatement = Statement())
			{
				stmtlist.EmplaceBack(*pStatement);
			}
			else if (m_state.flags.IsSet(Flags::Synchronize))
			{
				Synchronize();
			}
		}

		if (Match(TokenType::Return))
		{
			if (Optional<AST::Statement::Base*> pStatementReturn = StatementReturn())
			{
				stmtlist.EmplaceBack(*pStatementReturn);
			}
			else if (m_state.flags.IsSet(Flags::Synchronize))
			{
				Synchronize();
			}
		}
	}

	// explist ::= exp {‘,’ exp}
	void Parser::ParseExprlist(AST::Expressions& exprlist)
	{
		if (Optional<AST::Expression::Base*> pExpression = Expression())
		{
			exprlist.EmplaceBack(*pExpression);
		}

		while (Match(TokenType::Comma))
		{
			if (Optional<AST::Expression::Base*> pExpression = Expression())
			{
				exprlist.EmplaceBack(*pExpression);
			}
		}
	}

	// explist ::= exp {‘,’ exp}
	void Parser::ParseExprlist(AST::Expressions& exprlist, ArrayView<ReferenceWrapper<AST::Expression::Base>> varlist)
	{
		Assert(varlist.HasElements());
		if (Optional<AST::Expression::Base*> pExpression = Expression(*varlist[0]))
		{
			exprlist.EmplaceBack(*pExpression);
		}
		varlist++;

		while (Match(TokenType::Comma))
		{
			if (varlist.HasElements())
			{
				if (Optional<AST::Expression::Base*> pExpression = Expression(*varlist[0]))
				{
					exprlist.EmplaceBack(*pExpression);
				}
				varlist++;
			}
			else
			{
				if (Optional<AST::Expression::Base*> pExpression = Expression())
				{
					exprlist.EmplaceBack(*pExpression);
				}
			}
		}
	}

	// args ::=  ‘(’ [explist] ‘)’ | tableconstructor | LiteralString
	void Parser::ParseArgslist(AST::Expressions& argslist)
	{
		if (Match(TokenType::LeftParentheses))
		{
			if (!Check(TokenType::RightParentheses))
			{
				ParseExprlist(argslist);
				if (UNLIKELY_ERROR(argslist.GetSize() >= 255))
				{
					Error(SCRIPT_STRING_LITERAL("Expected less than 255 arguments in argslist"));
				}
			}
			if (UNLIKELY_ERROR(!Match(TokenType::RightParentheses)))
			{
				Error(SCRIPT_STRING_LITERAL("Expected ')' after '(' in argslist"));
			}
		}
		else if (Check(TokenType::LeftBrace))
		{
			// argslist.EmplaceBack(*TableConstructor());
		}
		else if (Match(TokenType::String))
		{
			argslist.EmplaceBack(m_graph.EmplaceNode<AST::Expression::Literal>(Previous().ToAny()));
		}
		else
		{
			Error(SCRIPT_STRING_LITERAL("Not a valid argslist"));
		}
	}

	Scripting::Type Parser::ParseVariableType()
	{
		InlineVector<Reflection::TypeDefinition, 1> allowedVariantTypes;
		do
		{
			if (Match(TokenType::Function))
			{
				// TODO: Parse signature
				allowedVariantTypes.EmplaceBack(Reflection::TypeDefinition::Get<VM::DynamicFunction>());
			}
			else if (Match(TokenType::Identifier))
			{
				const Token& name = Previous();
				const TokenType tokenType = GetTokenType(name.literal);
				Reflection::TypeDefinition typeDefinition = GetTokenTypeDefinition(tokenType);
				if (typeDefinition.IsValid())
				{
					allowedVariantTypes.EmplaceBack(typeDefinition);
				}
				else
				{
					return {};
				}
			}
			else if (Match(Array{TokenType::Number,       TokenType::Float,       TokenType::Integer,
			                     TokenType::Boolean,      TokenType::String,      TokenType::Null,
			                     TokenType::Component2D,  TokenType::Component3D, TokenType::ComponentSoftReference,
			                     TokenType::Vec2i,        TokenType::Vec2f,       TokenType::Vec2b,
			                     TokenType::Vec3i,        TokenType::Vec3f,       TokenType::Vec3b,
			                     TokenType::Vec4i,        TokenType::Vec4f,       TokenType::Vec4b,
			                     TokenType::Color,        TokenType::Rotation2D,  TokenType::Rotation3D,
			                     TokenType::Transform2D,  TokenType::Transform3D, TokenType::Asset,
			                     TokenType::TextureAsset, TokenType::Tag,         TokenType::RenderStage}))
			{
				allowedVariantTypes.EmplaceBack(GetTokenTypeDefinition(Previous().type));
			}
			else if (Check(TokenType::LeftBrace))
			{
				Assert(false, "TODO: Parse tables");
				Error("Table return types not supported yet");
				return {};
			}
			else
			{
				allowedVariantTypes.EmplaceBack(Reflection::TypeDefinition::Get<Any>());
			}
		} while (Match(TokenType::Pipe));

		if (allowedVariantTypes.GetSize() == 1)
		{
			return Move(allowedVariantTypes[0]);
		}
		else
		{
			return Reflection::DynamicTypeDefinition::MakeVariant(allowedVariantTypes);
		}
	}

	// namelist ::= Name {‘,’ Name}
	void Parser::ParseNamelist(AST::Expression::Variable::Tokens& namelist, Optional<Token>&& pName)
	{
		auto parseName = [this](AST::Expression::Variable::Tokens& namelist)
		{
			if (UNLIKELY_ERROR(!Match(TokenType::Identifier)))
			{
				Error(SCRIPT_STRING_LITERAL("Expected a name for namelist"));
				return;
			}
			const Token& name = Previous();
			Scripting::Type variableType;
			if (!ReachedEnd() && Match(TokenType::Colon))
			{
				variableType = ParseVariableType();
			}
			else
			{
				variableType = Reflection::TypeDefinition::Get<nullptr_type>();
			}

			namelist.EmplaceBack(VariableToken{Token(name), Move(variableType)});
		};

		if (pName.IsInvalid())
		{
			parseName(namelist);
		}
		else
		{
			namelist.EmplaceBack(VariableToken{Move(*pName), Reflection::TypeDefinition::Get<nullptr_type>()});
		}

		while (!ReachedEnd() && Check(TokenType::Comma))
		{
			if (PeekNext().type == TokenType::DotDotDot)
			{
				// ', ...' should be parsed by FunctionBody
				break;
			}
			Advance();
			parseName(namelist);
		}
	}

	void Parser::ParseReturnTypes(AST::Expression::Variable::Tokens& returnValues)
	{
		auto parseReturnType = [this](AST::Expression::Variable::Tokens& returnValues)
		{
			if (Check(TokenType::Identifier))
			{
				const Token& name = Peek();
				const TokenType tokenType = GetTokenType(name.literal);
				if (tokenType != TokenType::Invalid)
				{
					// No name specified
					// TODO: Generate a unique name?
					returnValues.EmplaceBack(VariableToken{Token(TokenType::Identifier), ParseVariableType()});
				}
				else
				{
					Advance();
					returnValues.EmplaceBack(VariableToken{Token(name), ParseVariableType()});
				}
			}
			else
			{
				// No name specified
				// TODO: Generate a unique name?
				returnValues.EmplaceBack(VariableToken{Token(TokenType::Identifier), ParseVariableType()});
			}
		};
		parseReturnType(returnValues);

		while (!ReachedEnd() && Check(TokenType::Comma))
		{
			Advance();
			parseReturnType(returnValues);
		}
	}

	bool Parser::StateView::Match(TokenType type)
	{
		if (Check(type))
		{
			Advance();
			return true;
		}
		return false;
	}

	bool Parser::Match(TokenType type)
	{
		return m_state.Match(type);
	}

	bool Parser::StateView::Match(ArrayView<TokenType, uint8> types)
	{
		if (Check(types))
		{
			Advance();
			return true;
		}
		return false;
	}

	bool Parser::Match(ArrayView<TokenType, uint8> types)
	{
		return m_state.Match(types);
	}

	bool Parser::StateView::Check(TokenType type) const
	{
		if (ReachedEnd())
		{
			return false;
		}
		return Peek().type == type;
	}

	bool Parser::Check(TokenType type) const
	{
		return m_state.Check(type);
	}

	bool Parser::StateView::Check(ArrayView<TokenType, uint8> types) const
	{
		if (ReachedEnd())
		{
			return false;
		}
		return types.Contains(Peek().type);
	}

	bool Parser::Check(ArrayView<TokenType, uint8> types) const
	{
		return m_state.Check(types);
	}

	void Parser::StateView::Advance()
	{
		if (!ReachedEnd())
		{
			++current;
		}
	}

	void Parser::Advance()
	{
		m_state.Advance();
	}

	void Parser::StateView::Reverse()
	{
		if (current != begin)
		{
			--current;
		}
	}

	void Parser::Reverse()
	{
		m_state.Reverse();
	}

	bool Parser::ReachedEnd() const
	{
		return m_state.ReachedEnd();
	}

	const Token& Parser::StateView::Peek() const
	{
		return *current;
	}

	const Token& Parser::Peek() const
	{
		return m_state.Peek();
	}

	const Token& Parser::StateView::PeekNext() const
	{
		if (ReachedEnd())
		{
			return *current;
		}
		auto nextIt = current;
		++nextIt;
		return *nextIt;
	}

	const Token& Parser::PeekNext() const
	{
		return m_state.PeekNext();
	}

	const Token& Parser::StateView::Previous() const
	{
		TokenListType::const_iterator it = current;
		--it;
		return *it;
	}

	const Token& Parser::Previous() const
	{
		return m_state.Previous();
	}

	void Parser::Error(StringType::ConstView error)
	{
		const Token& token = Peek();
		System::Get<Log>().Error(token.sourceLocation, error);
		m_state.flags.Set(Flags::Error | Flags::Synchronize);
	}

	void Parser::Synchronize()
	{
		m_state.flags.Clear(Flags::Synchronize);

		while (!ReachedEnd())
		{
			switch (Peek().type)
			{
				case TokenType::Semicolon:
					[[fallthrough]];
				case TokenType::End:
					[[fallthrough]];
				case TokenType::Do:
					[[fallthrough]];
				case TokenType::For:
					[[fallthrough]];
				case TokenType::Function:
					[[fallthrough]];
				case TokenType::If:
					[[fallthrough]];
				case TokenType::Return:
					[[fallthrough]];
				case TokenType::While:
					return;
				default:
					break;
			}

			Advance();
		}
	}

	[[maybe_unused]] const bool wasArrayVariableTypeRegistered = Reflection::Registry::RegisterType<ArrayVariableType>();
}
