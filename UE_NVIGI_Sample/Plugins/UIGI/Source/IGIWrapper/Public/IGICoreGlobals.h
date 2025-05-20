/*
 * SPDX-FileCopyrightText: Copyright (c) Space Kiwi Studio, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace IGI
{
	/** Check if IGI PACK Folder Exist Dynamically */
#if IGI_CORE_BINARY_EXISTS
	/** Path to NVIGI DLL found */
	const FString				GIGI_CORE_BINARIES_PATH	{ IGI_CORE_BINARIES_PATH };			// Value from nvigi Module
	const FString				GIGI_CORE_DLL_NAME		{ IGI_CORE_BINARY_NAME };		// Value from nvigi Module
	const FString				GIGI_BASE_MODELS_PATH	{ IGI_CORE_MODELS_PATH };		// Value from nvigi Module
	constexpr bool				GIGI_CORE_DLL_EXIST		{ IGI_CORE_BINARY_EXISTS };			// Value from nvigi Module
#else
	/** Path to NVIGI DLL not found */
	const FString				GIGI_CORE_BINARIES_PATH	{ TEXT("IGI Path not found!") };	// Value from nvigi Module
	const FString				GIGI_CORE_DLL_NAME		{ TEXT("IGI Path not found!") };	// Value from nvigi Module
	const FString				GIGI_BASE_MODELS_PATH	{ TEXT("IGI Path not found!") };	// Value from nvigi Module
	constexpr bool				GIGI_CORE_DLL_EXIST		{ IGI_CORE_BINARY_EXISTS };			// Value from nvigi Module
#endif
	/////////////////////////////////////////////////

	/** Check is it's a Build Shipping Dynamically */
#if UE_BUILD_SHIPPING
	/** Hide console when it's a Build Shipping */
	constexpr bool				GIGI_SHOW_CONSOLE		{ false };
#else
	/** Show console when isn't a Build Shipping */
	constexpr bool				GIGI_SHOW_CONSOLE		{ true };
#endif
	/////////////////////////////////////////////////
	
	/** Check for DLL Sig only for Windows in Production */
#if PLATFORM_WINDOWS && UE_BUILD_SHIPPING 
	inline constinit bool 		GIGI_USE_DLL_SIG		{ true };							// Only for Windows 
#else
	inline constinit bool 		GIGI_USE_DLL_SIG		{ false };							// Only for Windows
#endif
	/////////////////////////////////////////////////
	
	/** Use CiG CUDA */
	inline constinit bool 		GIGI_USE_CIG_CUDA		{ false };
	
}
