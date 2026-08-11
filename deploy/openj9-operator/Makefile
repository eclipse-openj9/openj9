OPERATOR_SDK_RELEASE_VERSION ?= v0.15.2
OPERATOR_IMAGE ?= chrisc66/openj9-opeartor
OPERATOR_IMAGE_TAG ?= latest

WATCH_NAMESPACE ?= operator-test
OPERATOR_NAMESPACE ?= ${WATCH_NAMESPACE}

# Get source files, ignore vendor directory
SRC_FILES := $(shell find . -type f -name '*.go' -not -path "./vendor/*")

.DEFAULT_GOAL := help

.PHONY: help tidy build build-and-push-image gofmt golint clean install-crd install-rbac install-operator install-all uninstall-all

help:
	@grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'

tidy: ## Clean up Go modules by adding missing and removing unused modules
	go mod tidy

build: ## Install and compile the operator
	go install ./cmd/manager
	go build ./cmd/manager

build-and-push-image: setup ## Build operator Docker image and tag with "${OPERATOR_IMAGE}:${OPERATOR_IMAGE_TAG}"
	operator-sdk build ${OPERATOR_IMAGE}:${OPERATOR_IMAGE_TAG}
	docker push ${OPERATOR_IMAGE}:${OPERATOR_IMAGE_TAG}

gofmt: ## Format the Go code with `gofmt`
	@gofmt -s -l -w $(SRC_FILES)

golint: ## Run linter on operator code
	for file in $(SRC_FILES); do \
		golint $${file}; \
		if [ -n "$$(golint $${file})" ]; then \
			exit 1; \
		fi; \
	done

clean: ## Clean binary artifacts
	rm -rf build/_output

install-crd: ## Install operator CRD in the deploy directory
	kubectl apply -f deploy/crds/open.j9_runtimecomponents_crd.yaml

install-rbac: ## Install RBAC objects required for the operator
	kubectl apply -f deploy/service_account.yaml
	kubectl apply -f deploy/role.yaml
	kubectl apply -f deploy/role_binding.yaml

install-operator: ## Install operator in the ${OPERATOR_NAMESPACE} namespace and watches ${WATCH_NAMESPACE} namespace. ${WATCH_NAMESPACE} defaults to `default`. ${OPERATOR_NAMESPACE} defaults to ${WATCH_NAMESPACE}
	kubectl apply -n ${OPERATOR_NAMESPACE} -f deploy/operator.yaml

install-all: install-rbac install-crd install-operator ## Install operator RBAC objects, CRD and operator deployment

uninstall-all: ## Uninstall opeartor deployment, CRD, and RBAC objects
	kubectl delete -n ${OPERATOR_NAMESPACE} -f deploy/operator.yaml
	kubectl delete -f deploy/crds/open.j9_runtimecomponents_crd.yaml
	kubectl delete -f deploy/role_binding.yaml
	kubectl delete -f deploy/role.yaml
	kubectl delete -f deploy/service_account.yaml
