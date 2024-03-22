#pragma once
// Rename to be consistent with naming_convention
using Bool	  = bool;
using Char	  = char;
using Void	  = void;
			  
using Int8	  = int8_t;
using Int16	  = int16_t;
using Int32	  = int32_t;
using Int64	  = int64_t;
			  
using UInt8	  = uint8_t;
using UInt16  = uint16_t;
using UInt32  = uint32_t;
using UInt64  = uint64_t;
			  
using Float32 = float;
using Float64 = double;

// Types that can be changed to own types instead of types from libraries
using String	   = std::string;

template<typename Type>
using Limits	   = std::numeric_limits<Type>;
template<typename Type>
using Optional	   = std::optional<Type>;
template<typename Type>
using List		   = std::list<Type>;
template<typename Type>
using DynamicArray = std::vector<Type>;
template<typename Type>
using Set		   = std::set<Type>;
template<typename Type, UInt64 Count>
using Array		   = std::array<Type, Count>;
template<typename FirstType, typename SecondType>
using Pair		   = std::pair<FirstType, SecondType>;
template<typename KeyType, typename ValueType>
using HashMap	   = std::unordered_map<KeyType, ValueType>;
template<typename KeyType, typename ValueType>
using Map		   = std::map<KeyType, ValueType>;

using UVector2	   = glm::uvec2;
using UVector3	   = glm::uvec3;
using UVector4	   = glm::uvec4;
				   
using IVector2	   = glm::ivec2;
using IVector3	   = glm::ivec3;
using IVector4	   = glm::ivec4;
				   
using FVector2	   = glm::vec2;
using FVector3	   = glm::vec3;
using FVector4	   = glm::vec4;
				   
using DVector2	   = glm::dvec2;
using DVector3	   = glm::dvec3;
using DVector4	   = glm::dvec4;
				   
using DMatrix2	   = glm::dmat2;
using DMatrix3	   = glm::dmat3;
using DMatrix4	   = glm::dmat4;
				   
using FMatrix2	   = glm::mat2;
using FMatrix3	   = glm::mat3;
using FMatrix4	   = glm::mat4;