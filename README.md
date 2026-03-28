# Development System

Status: active
Audience: PM, engineers, AI agents
Purpose: keep development-system assets separate from business repositories.

## Structure

- `shared`
  - reusable cross-project rules and templates
- `projects`
  - project-specific development systems, task boards, and execution rules

## Current project folders

- `projects/houjinongfuai`
  - development-system workspace for the `houjinongfuai` project

## Shared rules

- `shared/global-rules`
  - cross-project task classification and other shared development-system rules

## Separation rule

- business repositories hold source code and business-facing documents
- development-system workspace holds onboarding, dispatch, workflow, governance, and execution rules
- business requirements are confirmed in the business repository first
- AI execution tasks are decomposed afterwards inside the project development-system workspace
